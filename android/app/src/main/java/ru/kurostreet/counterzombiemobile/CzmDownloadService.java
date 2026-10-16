/* User-started foreground transfer; the engine owns downloading and disk writes. */
package ru.kurostreet.counterzombiemobile;
import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.os.*;

public final class CzmDownloadService extends Service {
    private static final String CHANNEL="czm_downloads",CANCEL="czm.cancel_download";
    private static final int NOTICE=2701;
    private final Handler ui=new Handler(Looper.getMainLooper());
    private PowerManager.WakeLock wake;
    private int session;
    private long started;
    private NotificationManager notifications;
    private final Runnable poll=new Runnable(){public void run(){
        CzmDownloads.Snapshot s=CzmDownloads.snapshot();long now=SystemClock.elapsedRealtime();
        if(s.session!=session){finish(null);return;}
        if(s.status==1||s.status==3){finish(CzmDownloads.ru()?"Файлы готовы. Открой игру для входа на сервер.":"Files are ready. Open the game to join the server.");return;}
        if(s.status==2){finish(CzmDownloads.ru()?"Загрузка прервана. Открой игру, чтобы увидеть причину.":"Download stopped. Open the game to see why.");return;}
        if(now-started>25*60*1000L||now-s.updated>30000){CzmDownloads.cancel(session);finish(CzmDownloads.ru()?"Загрузка приостановлена. Открой игру для продолжения.":"Download paused. Open the game to continue.");return;}
        notifications.notify(NOTICE,notification(null));ui.postDelayed(this,1000);
    }};
    @Override public void onCreate(){super.onCreate();notifications=(NotificationManager)getSystemService(NOTIFICATION_SERVICE);
        if(Build.VERSION.SDK_INT>=26){NotificationChannel c=new NotificationChannel(CHANNEL,CzmDownloads.ru()?"Загрузка файлов игры":"Game downloads",NotificationManager.IMPORTANCE_LOW);c.setSound(null,null);notifications.createNotificationChannel(c);}
    }
    @Override public int onStartCommand(Intent intent,int flags,int startId){
        if(intent==null){stopSelf();return START_NOT_STICKY;}
        if(CANCEL.equals(intent.getAction())){if(session==intent.getIntExtra("session",-1)){CzmDownloads.cancel(session);finish(null);}return START_NOT_STICKY;}
        session=intent.getIntExtra("session",0);CzmDownloads.Snapshot s=CzmDownloads.snapshot();if(s.session!=session||s.status!=0){stopSelf();return START_NOT_STICKY;}
        try{
            if(Build.VERSION.SDK_INT>=29)startForeground(NOTICE,notification(null),ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);else startForeground(NOTICE,notification(null));
            if(wake==null){PowerManager p=(PowerManager)getSystemService(POWER_SERVICE);wake=p.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,getPackageName()+":server-download");wake.setReferenceCounted(false);wake.acquire(26*60*1000L);}
        }catch(RuntimeException e){CzmDownloads.cancel(session);stopSelf();return START_NOT_STICKY;}
        started=SystemClock.elapsedRealtime();ui.removeCallbacks(poll);ui.post(poll);return START_NOT_STICKY;
    }
    private Notification notification(String finished){
        CzmDownloads.Snapshot s=CzmDownloads.snapshot();boolean ru=CzmDownloads.ru();
        PendingIntent open=PendingIntent.getActivity(this,NOTICE,new Intent(this,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder b=Build.VERSION.SDK_INT>=26?new Notification.Builder(this,CHANNEL):new Notification.Builder(this);
        b.setSmallIcon(R.drawable.czm_notification).setContentTitle(finished==null?(ru?"CZM · загрузка ":"CZM · downloading ")+s.percent+"%":"Counter Zombie Mobile")
            .setContentText(finished==null?s.label:finished).setContentIntent(open).setOnlyAlertOnce(true).setOngoing(finished==null).setAutoCancel(finished!=null).setCategory(Notification.CATEGORY_PROGRESS).setPriority(Notification.PRIORITY_LOW);
        if(finished==null){
            PendingIntent cancel=PendingIntent.getService(this,session,new Intent(this,CzmDownloadService.class).setAction(CANCEL).putExtra("session",session),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
            b.setProgress(100,s.percent,s.percent<=0).addAction(0,ru?"Отмена":"Cancel",cancel);
        }return b.build();
    }
    private void finish(String message){ui.removeCallbacks(poll);stopForeground(true);release();if(message!=null)notifications.notify(NOTICE,notification(message));stopSelf();}
    private void release(){if(wake!=null&&wake.isHeld())wake.release();wake=null;}
    @Override public void onTimeout(int startId,int type){CzmDownloads.cancel(session);finish(CzmDownloads.ru()?"Открой игру для продолжения загрузки.":"Open the game to continue downloading.");}
    @Override public void onTaskRemoved(Intent intent){CzmDownloads.cancel(session);finish(null);}
    @Override public void onDestroy(){ui.removeCallbacks(poll);CzmDownloads.stopped(session);release();super.onDestroy();}
    @Override public IBinder onBind(Intent intent){return null;}
}
