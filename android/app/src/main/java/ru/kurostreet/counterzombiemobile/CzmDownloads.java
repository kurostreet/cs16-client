/* Native transfer state; UI and notifications never invent download progress. */
package ru.kurostreet.counterzombiemobile;
import android.app.Activity;
import android.content.*;
import android.os.Build;
import android.os.SystemClock;
import android.widget.Toast;
import org.libsdl.app.SDLActivity;

final class CzmDownloads {
    static final class Snapshot {
        final int session,status,percent;final String label;final long updated;
        Snapshot(int s,int t,int p,String l,long now){session=s;status=t;percent=Math.max(0,Math.min(100,p));label=l;updated=now;}
    }
    private static Snapshot current=new Snapshot(0,2,0,"",0);
    private static boolean requested,cancel;
    static synchronized void progress(int session,int status,int percent,String label){
        if(session<=0||status<0||status>3)return;
        if(current.session!=session){requested=false;cancel=false;}
        current=new Snapshot(session,status,percent,label,SystemClock.elapsedRealtime());
    }
    static synchronized Snapshot snapshot(){return current;}
    static synchronized boolean waitForForeground(){return requested;}
    static synchronized boolean cancelEvent(){boolean value=cancel;cancel=false;return value;}
    static synchronized void cancel(int session){if(current.session==session){cancel=true;requested=false;}}
    static synchronized void declined(){requested=false;}
    static synchronized void stopped(int session){if(current.session==session)requested=false;}
    static void request(){
        android.content.Context ctx=SDLActivity.getContext();if(!(ctx instanceof CzmGameActivity))return;
        synchronized(CzmDownloads.class){if(current.status!=0||requested)return;requested=true;}
        ((Activity)ctx).runOnUiThread(()->((CzmGameActivity)ctx).prepareBackgroundDownload());
    }
    static void start(Activity a){
        Snapshot s=snapshot();if(s.status!=0){declined();return;}
        synchronized(CzmDownloads.class){requested=true;}
        Intent intent=new Intent(a,CzmDownloadService.class).putExtra("session",s.session);
        try{if(Build.VERSION.SDK_INT>=26)a.startForegroundService(intent);else a.startService(intent);a.moveTaskToBack(true);}
        catch(RuntimeException e){declined();Toast.makeText(a,ru()?"Не удалось продолжить в фоне. Оставь игру открытой.":"Could not continue in the background. Keep the game open.",Toast.LENGTH_LONG).show();}
    }
    static void foreground(Context c){declined();c.stopService(new Intent(c,CzmDownloadService.class));}
    static boolean ru(){return java.util.Locale.getDefault().getLanguage().equals("ru");}
}
