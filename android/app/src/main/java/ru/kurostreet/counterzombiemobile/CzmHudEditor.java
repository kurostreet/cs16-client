/* Staged, draggable HUD editor. GPL-3.0-or-later. */
package ru.kurostreet.counterzombiemobile;

import android.app.Activity;
import android.graphics.*;
import android.view.*;
import android.widget.*;
import org.json.JSONObject;
import org.libsdl.app.SDLActivity;
import java.io.File;
import java.nio.charset.StandardCharsets;

final class CzmHudEditor {
    static final String[] KEYS={"hp","armor","time","money","ammo"};
    static final float[][] DEFAULTS={{.085f,.94f},{.25f,.94f},{.5f,.94f},{.88f,.82f},{.86f,.94f}};
    private static String pending="";
    private static CzmDialog editor;
    static synchronized byte[] event() {
        if(pending.isEmpty()) return null;
        byte[] b=pending.getBytes(StandardCharsets.UTF_8);pending="";return b;
    }
    static float clamp(double v,float low,float high,float fallback) {
        return (!Double.isNaN(v)&&!Double.isInfinite(v))?Math.max(low,Math.min(high,(float)v)):fallback;
    }
    static void show(byte[] body,boolean ru) {
        if(!(SDLActivity.getContext() instanceof Activity)) return;
        Activity a=(Activity)SDLActivity.getContext();
        a.runOnUiThread(()->{
            if(a.isFinishing() || editor!=null) return;
            try { new Session(a,new JSONObject(new String(body,StandardCharsets.UTF_8)),ru).open(); }
            catch(Exception e) { Toast.makeText(a,ru?"Не удалось открыть HUD":"Could not open HUD",Toast.LENGTH_SHORT).show(); }
        });
    }
    private static final class Session {
        final Activity a;final boolean ru;final float[][] blocks=new float[5][4];
        final CzmDialog d; final Preview preview;
        final SeekBar size,alpha; final TextView metrics;
        final Button[] styles=new Button[3],items=new Button[5];
        int style,selected;boolean selecting;
        Session(Activity activity,JSONObject data,boolean russian) {
            a=activity;ru=russian;style=(int)clamp(data.optDouble("style"),0,2,0);
            JSONObject all=data.optJSONObject("blocks");
            for(int i=0;i<5;i++) {
                JSONObject p=all==null?null:all.optJSONObject(KEYS[i]);
                blocks[i][0]=p==null?DEFAULTS[i][0]:clamp(p.optDouble("x"),.01f,.99f,DEFAULTS[i][0]);
                blocks[i][1]=p==null?DEFAULTS[i][1]:clamp(p.optDouble("y"),.01f,.99f,DEFAULTS[i][1]);
                blocks[i][2]=p==null?1:clamp(p.optDouble("scale"),.65f,1.6f,1);
                blocks[i][3]=p==null?1:clamp(p.optDouble("opacity"),.25f,1,1);
            }
            d=new CzmDialog(a,ru?"Интерфейс в матче":"In-match HUD",ru?"Перетаскивай показатели на предпросмотре":"Drag the indicators in the preview",ru?"Сохранить":"Save",ru?"Отмена":"Cancel",ru,false);
            LinearLayout row=new LinearLayout(a);d.body.addView(row);
            String[] names=ru?new String[]{"Классический","Компактный","CZM"}:new String[]{"Classic","Compact","CZM"};
            for(int i=0;i<3;i++) {final int n=i;styles[i]=d.button(names[i],false);row.addView(styles[i],new LinearLayout.LayoutParams(0,CzmDialog.dp(a,38),1));styles[i].setOnClickListener(v->{style=n;refresh();});}
            preview=new Preview(a);d.body.addView(preview,new LinearLayout.LayoutParams(-1,CzmDialog.dp(a,155)));
            row=new LinearLayout(a);d.body.addView(row);
            names=ru?new String[]{"Жизни","Броня","Время","Деньги","Патроны"}:new String[]{"Health","Armor","Time","Credits","Ammo"};
            for(int i=0;i<5;i++) {final int n=i;items[i]=d.button(names[i],false);items[i].setTextSize(11);row.addView(items[i],new LinearLayout.LayoutParams(0,CzmDialog.dp(a,36),1));items[i].setOnClickListener(v->{selected=n;select();});}
            metrics=d.note("");size=new SeekBar(a);size.setMax(95);alpha=new SeekBar(a);alpha.setMax(75);
            d.body.addView(size,new LinearLayout.LayoutParams(-1,CzmDialog.dp(a,32)));d.body.addView(alpha,new LinearLayout.LayoutParams(-1,CzmDialog.dp(a,32)));
            size.setContentDescription(ru?"Размер выбранного блока":"Selected block size");alpha.setContentDescription(ru?"Прозрачность выбранного блока":"Selected block opacity");
            SeekBar.OnSeekBarChangeListener listener=new SeekBar.OnSeekBarChangeListener(){
                public void onProgressChanged(SeekBar bar,int value,boolean user){if(selecting)return;blocks[selected][bar==size?2:3]=(value+(bar==size?65:25))/100f;refresh();}
                public void onStartTrackingTouch(SeekBar bar){} public void onStopTrackingTouch(SeekBar bar){}
            };size.setOnSeekBarChangeListener(listener);alpha.setOnSeekBarChangeListener(listener);
            d.link(ru?"Сбросить расположение и размеры":"Reset positions and sizes",v->{for(int i=0;i<5;i++){blocks[i][0]=DEFAULTS[i][0];blocks[i][1]=DEFAULTS[i][1];blocks[i][2]=blocks[i][3]=1;}select();});
            d.submit.setOnClickListener(v->{
                try {
                    JSONObject out=new JSONObject(),allBlocks=new JSONObject();out.put("style",style).put("blocks",allBlocks);
                    for(int i=0;i<5;i++)allBlocks.put(KEYS[i],new JSONObject().put("x",blocks[i][0]).put("y",blocks[i][1]).put("scale",blocks[i][2]).put("opacity",blocks[i][3]));
                    synchronized(CzmHudEditor.class){pending=out.toString();}d.dismiss();
                }catch(Exception ignored){}
            });
            d.setOnDismissListener(v->{if(editor==d)editor=null;});
        }
        void open(){editor=d;d.show();android.util.DisplayMetrics m=a.getResources().getDisplayMetrics();d.getWindow().setLayout(Math.min(CzmDialog.dp(a,720),m.widthPixels-CzmDialog.dp(a,24)),m.heightPixels-CzmDialog.dp(a,24));select();}
        void select(){selecting=true;size.setProgress(Math.round(blocks[selected][2]*100)-65);alpha.setProgress(Math.round(blocks[selected][3]*100)-25);selecting=false;refresh();}
        void refresh(){
            for(int i=0;i<3;i++)styles[i].setTextColor(style==i?Color.WHITE:CzmDialog.MUTED);
            for(int i=0;i<5;i++)items[i].setTextColor(selected==i?Color.WHITE:CzmDialog.MUTED);
            metrics.setText((ru?"Размер: ":"Size: ")+Math.round(blocks[selected][2]*100)+"%   ·   "+(ru?"Непрозрачность: ":"Opacity: ")+Math.round(blocks[selected][3]*100)+"%");preview.invalidate();
        }
        final class Preview extends View {
            final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);final RectF[] rects=new RectF[5];Typeface font=Typeface.DEFAULT_BOLD;
            final String[] values={"2904","100","2:38","16000","30 / 90"};boolean dragging;float dx,dy;
            Preview(Activity a){super(a);for(int i=0;i<5;i++)rects[i]=new RectF();try{font=Typeface.createFromAsset(a.getAssets(),"gfx/fonts/czm/InterDisplay-Bold.otf");}catch(Exception ignored){}setContentDescription(ru?"Предпросмотр HUD. Выбери показатель и перетащи":"HUD preview. Select and drag an indicator");}
            @Override protected void onDraw(Canvas c){
                c.drawColor(Color.rgb(27,36,46));paint.setColor(Color.rgb(37,49,61));
                for(int i=0;i<7;i++)c.drawRect(i*getWidth()/7f,0,i*getWidth()/7f+1,getHeight(),paint);
                paint.setColor(Color.rgb(57,72,86));c.drawLine(getWidth()/2f-8,getHeight()/2f,getWidth()/2f+8,getHeight()/2f,paint);c.drawLine(getWidth()/2f,getHeight()/2f-8,getWidth()/2f,getHeight()/2f+8,paint);
                for(int i=0;i<5;i++){
                    float unit=getHeight()/480f*blocks[i][2],h=(style==1?32:style==2?52:44)*unit,w=Math.min(getWidth()*.38f,(34+Math.max(5,values[i].length())*17)*unit);
                    h=Math.min(h,getHeight()*.17f);float x=clamp(blocks[i][0]*getWidth()-w/2,getWidth()*.012f,getWidth()*.988f-w,0),y=clamp(blocks[i][1]*getHeight()-h/2,getHeight()*.018f,getHeight()*.982f-h,0);RectF p=rects[i];p.set(x,y,x+w,y+h);
                    paint.setAlpha(255);paint.setStyle(Paint.Style.FILL);if(style!=1){paint.setColor(Color.argb((int)(220*blocks[i][3]),10,14,22));c.drawRoundRect(p,style==2?3:0,style==2?3:0,paint);}
                    paint.setColor(Color.argb((int)(255*blocks[i][3]),224,37,50));if(style==2)c.drawRect(x,y,x+w,y+Math.max(1,h/18),paint);else c.drawRect(x,style==1?y+h-1:y,x+(style==1?w:2),style==1?y+h:y+h,paint);
                    float s=h*.36f,ix=x+h*.18f,iy=y+(h-s)/2;paint.setColor(Color.argb((int)(255*blocks[i][3]),214,224,239));paint.setStrokeWidth(Math.max(1,s/5));
                    if(i==0){c.drawLine(ix+s/2,iy,ix+s/2,iy+s,paint);c.drawLine(ix,iy+s/2,ix+s,iy+s/2,paint);}else if(i==2){paint.setStyle(Paint.Style.STROKE);c.drawRect(ix,iy,ix+s,iy+s,paint);c.drawLine(ix+s/2,iy+2,ix+s/2,iy+s/2,paint);}else c.drawRect(ix,iy,ix+s,iy+s,paint);
                    paint.setStyle(Paint.Style.FILL);paint.setTypeface(font);float textH=h*(style==1?.7f:.52f);paint.setTextSize(textH);paint.setColor(Color.argb((int)(255*blocks[i][3]),246,249,255));float tx=ix+s+h*.15f;while(paint.measureText(values[i])>x+w-tx-2 && paint.getTextSize()>3)paint.setTextSize(paint.getTextSize()-.5f);c.drawText(values[i],tx,y+h*(style==2?.85f:.72f),paint);
                    if(style==2){paint.setTextSize(h*.16f);paint.setColor(Color.rgb(147,159,180));c.drawText(new String[]{"HEALTH","ARMOR","ROUND","CREDITS","AMMO"}[i],tx,y+h*.28f,paint);}
                    if(i==selected){paint.setColor(Color.WHITE);paint.setStrokeWidth(1);paint.setStyle(Paint.Style.STROKE);c.drawRect(x-2,y-2,x+w+2,y+h+2,paint);paint.setStyle(Paint.Style.FILL);}
                }
            }
            @Override public boolean onTouchEvent(MotionEvent e){
                if(e.getActionMasked()==MotionEvent.ACTION_DOWN){
                    for(int i=4;i>=0;i--)if(rects[i].contains(e.getX(),e.getY())){selected=i;dragging=true;dx=e.getX()-blocks[i][0]*getWidth();dy=e.getY()-blocks[i][1]*getHeight();getParent().requestDisallowInterceptTouchEvent(true);select();return true;}
                    return false;
                }
                if(!dragging)return false;
                if(e.getActionMasked()==MotionEvent.ACTION_MOVE){blocks[selected][0]=clamp((e.getX()-dx)/getWidth(),.01f,.99f,DEFAULTS[selected][0]);blocks[selected][1]=clamp((e.getY()-dy)/getHeight(),.01f,.99f,DEFAULTS[selected][1]);invalidate();}
                if(e.getActionMasked()==MotionEvent.ACTION_UP || e.getActionMasked()==MotionEvent.ACTION_CANCEL){dragging=false;getParent().requestDisallowInterceptTouchEvent(false);performClick();}
                return true;
            }
            @Override public boolean performClick(){super.performClick();return true;}
        }
    }
}
