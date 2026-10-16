package ru.kurostreet.counterzombiemobile;
import android.app.*;
import android.appwidget.*;
import android.content.*;
import android.view.View;
import android.widget.RemoteViews;
import java.io.File;
import java.text.DateFormat;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

/** The heart list on Android home; stars belong to the separate in-game list. */
public final class CzmServerWidget extends AppWidgetProvider {
    static final String REFRESH="ru.kurostreet.counterzombiemobile.WIDGET_REFRESH";
    private static final ExecutorService WORK=Executors.newSingleThreadExecutor();
    private static final AtomicBoolean QUERYING=new AtomicBoolean();
    static List<String[]> hearts(Context c){
        ArrayList<String[]> list=new ArrayList<>();try{File f=new File(c.getFilesDir(),"game/cstrike/home_servers.lst");for(String line:SettingsData.read(f).split("\n")){List<String> t=SettingsData.tokens(line);if(t.size()==2&&ServerQuery.valid(t.get(0))&&(t.get(1).equals("gs")||t.get(1).equals("49")))list.add(new String[]{t.get(0),t.get(1)});if(list.size()==3)break;}}catch(Exception ignored){}return list;
    }
    @Override public void onReceive(Context c,Intent intent){
        if(REFRESH.equals(intent.getAction())||AppWidgetManager.ACTION_APPWIDGET_UPDATE.equals(intent.getAction())){
            int[] ids=AppWidgetManager.getInstance(c).getAppWidgetIds(new ComponentName(c,CzmServerWidget.class));
            if(!QUERYING.compareAndSet(false,true)){update(c,ids,false);return;}
            final PendingResult result=goAsync();WORK.execute(()->{try{update(c,ids,true);}finally{QUERYING.set(false);result.finish();}});return;
        }super.onReceive(c,intent);
    }
    @Override public void onUpdate(Context c,AppWidgetManager manager,int[] ids){update(c,ids,false);}
    @Override public void onAppWidgetOptionsChanged(Context c,AppWidgetManager manager,int id,android.os.Bundle options){update(c,new int[]{id},false);}
    @Override public void onDeleted(Context c,int[] ids){SharedPreferences.Editor e=c.getSharedPreferences("czm_widget",0).edit();for(int id:ids)e.remove("choice"+id);e.apply();}
    static void update(Context c,int[] ids,boolean query){
        List<String[]> pinned=hearts(c);SharedPreferences saved=c.getSharedPreferences("czm_widget",0);
        if(query)for(String[] server:pinned)try{ServerQuery.Info p=ServerQuery.query(server[0]);saved.edit().putString(server[0]+"name",p.name).putString(server[0]+"map",p.map).putString(server[0]+"count",p.players+" / "+p.max).putLong(server[0]+"time",System.currentTimeMillis()).apply();}catch(Exception ignored){}
        for(int id:ids){
            RemoteViews views=new RemoteViews(c.getPackageName(),R.layout.czm_server_widget);int[] rows={R.id.widget_server1,R.id.widget_server2,R.id.widget_server3};
            boolean compact=AppWidgetManager.getInstance(c).getAppWidgetOptions(id).getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_HEIGHT,180)<170;
            String only=saved.getString("choice"+id,"");int visible=0;long oldest=Long.MAX_VALUE;
            for(int i=0;i<3;i++){
                if(i>=pinned.size()||!only.isEmpty()&&!only.equals(pinned.get(i)[0])){views.setViewVisibility(rows[i],View.GONE);continue;}
                String[] item=pinned.get(i);String address=item[0];String title=saved.getString(address+"name",address),map=saved.getString(address+"map","");long time=saved.getLong(address+"time",0);oldest=Math.min(oldest,time);
                String count=saved.getString(address+"count","");boolean recent=time>0&&System.currentTimeMillis()-time<35*60*1000;
                views.setViewVisibility(rows[i],View.VISIBLE);views.setTextViewText(rows[i],"♥  "+title+(compact?"   ":"\n"+(map.isEmpty()?address:map)+"   ·   ")+(recent?count:Locale.getDefault().getLanguage().equals("ru")?"нет свежего ответа":"no recent reply"));
                Intent open=new Intent(c,MainActivity.class).putExtra("czm_server",address).putExtra("czm_protocol",item[1]).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                views.setOnClickPendingIntent(rows[i],PendingIntent.getActivity(c,id*10+i,open,PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE));visible++;
            }
            boolean ru=Locale.getDefault().getLanguage().equals("ru");views.setViewVisibility(R.id.widget_empty,visible==0?View.VISIBLE:View.GONE);
            views.setTextViewText(R.id.widget_empty,ru?"Отметь сервер сердечком в игре":"Mark a server with a heart in the game");
            views.setTextViewText(R.id.widget_time,oldest>0&&oldest!=Long.MAX_VALUE?(ru?"Обновлено ":"Updated ")+DateFormat.getTimeInstance(DateFormat.SHORT).format(new Date(oldest)):(ru?"Нажми ↻ для обновления":"Tap ↻ to refresh"));
            Intent refresh=new Intent(c,CzmServerWidget.class).setAction(REFRESH).putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS,new int[]{id});
            views.setOnClickPendingIntent(R.id.widget_refresh,PendingIntent.getBroadcast(c,id,refresh,PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE));
            Intent configure=new Intent(c,CzmWidgetConfigure.class).putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID,id);views.setOnClickPendingIntent(R.id.widget_title,PendingIntent.getActivity(c,100000+id,configure,PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE));
            AppWidgetManager.getInstance(c).updateAppWidget(id,views);
        }
    }
}
