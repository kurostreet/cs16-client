/* One accessible numeric input, rendered as six cells. GPL-3.0-or-later. */
package ru.kurostreet.counterzombiemobile;

import android.content.Context;
import android.graphics.*;
import android.text.InputFilter;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.view.MotionEvent;
import android.widget.EditText;

final class CzmCodeInput extends EditText {
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    CzmCodeInput(Context context,boolean ru){
        super(context);setSingleLine(true);setInputType(InputType.TYPE_CLASS_NUMBER);
        setKeyListener(DigitsKeyListener.getInstance("0123456789"));
        setFilters(new InputFilter[]{new InputFilter.LengthFilter(6)});
        setBackgroundColor(Color.TRANSPARENT);setTextColor(Color.TRANSPARENT);setCursorVisible(false);
        setPadding(0,0,0,0);setContentDescription(ru?"Шестизначный код из письма":"Six-digit email code");
        if(android.os.Build.VERSION.SDK_INT>=26)setImportantForAutofill(IMPORTANT_FOR_AUTOFILL_YES);
    }
    @Override protected void onSelectionChanged(int start,int end){super.onSelectionChanged(start,end);invalidate();}
    @Override protected void onDraw(Canvas canvas){
        float gap=getResources().getDisplayMetrics().density*6,width=(getWidth()-gap*5)/6f;
        String code=getText().toString();
        for(int i=0;i<6;i++){
            RectF r=new RectF(i*(width+gap),2,i*(width+gap)+width,getHeight()-2);
            paint.setStyle(Paint.Style.FILL);paint.setColor(Color.rgb(24,28,36));canvas.drawRoundRect(r,8,8,paint);
            paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(2);
            paint.setColor(hasFocus() && Math.min(getSelectionStart(),5)==i?CzmDialog.RED:Color.rgb(61,67,80));canvas.drawRoundRect(r,8,8,paint);
            if(i<code.length()){
                paint.setStyle(Paint.Style.FILL);paint.setColor(Color.WHITE);paint.setTypeface(Typeface.DEFAULT_BOLD);paint.setTextSize(Math.min(getHeight()*.52f,width*.65f));paint.setTextAlign(Paint.Align.CENTER);
                canvas.drawText(code.substring(i,i+1),r.centerX(),r.centerY()-(paint.ascent()+paint.descent())/2,paint);
            }
        }
    }
    @Override public boolean onTouchEvent(MotionEvent event){
        boolean handled=super.onTouchEvent(event);
        if(event.getActionMasked()==MotionEvent.ACTION_UP){float gap=getResources().getDisplayMetrics().density*6;float width=(getWidth()-gap*5)/6f;setSelection(Math.min(length(),Math.max(0,Math.min(5,(int)(event.getX()/(width+gap))))));invalidate();}
        return handled;
    }
}
