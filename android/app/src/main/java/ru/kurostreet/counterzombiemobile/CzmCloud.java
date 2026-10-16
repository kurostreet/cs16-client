/* Account-scoped cloud sync with revision conflicts and an offline queue. GPL-3.0-or-later. */
package ru.kurostreet.counterzombiemobile;
import android.app.Activity;
import android.content.*;
import android.widget.Toast;
import org.json.*;
import org.libsdl.app.SDLActivity;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;

final class CzmCloud {
    private static final String URL="https://counter-zombie.ru/api/v1/settings";
    private static final ScheduledExecutorService WORK=Executors.newSingleThreadScheduledExecutor();
    private static int player,generation,revision,nextApply,activeApply;
    private static boolean started,loaded,applying,conflict;
    private static String baseline="",pending="",state="guest";
    private static SharedPreferences prefs(){return SDLActivity.getContext().getSharedPreferences("czm_cloud",Context.MODE_PRIVATE);}
    static void account(int id){
        if(SDLActivity.getContext()==null)return;int gen=CzmAccount.generation();synchronized(CzmCloud.class){if(player==id&&generation==gen)return;
        player=id;generation=gen;loaded=applying=conflict=false;activeApply=0;baseline=pending="";revision=0;state=id>0?"loading":"guest";
        if(!started){started=true;WORK.scheduleWithFixedDelay(CzmCloud::tick,1,10,TimeUnit.SECONDS);}}
    }
    static synchronized void clear(){player=0;loaded=applying=conflict=false;activeApply=0;pending="";state="guest";}
    private static JSONObject body(String r)throws Exception {int i=r.indexOf('\n');return new JSONObject(r.substring(i+1));}
    private static int status(String r){try{return Integer.parseInt(r.substring(0,r.indexOf('\n')));}catch(Exception e){return 0;}}
    static byte[] event(){
        int gen=CzmAccount.generation();synchronized(CzmCloud.class){
        if(pending.isEmpty()||generation!=gen)return null;
        byte[] out=pending.getBytes(StandardCharsets.UTF_8);pending="";return out;}
    }
    static synchronized void applied(int request,boolean success){
        if(!applying||request!=activeApply)return;
        applying=false;activeApply=0;
        if(!success){loaded=false;conflict=true;state="apply_failed";return;}
        String root="p"+player+".";
        if(!prefs().edit().remove(root+"pending").putInt(root+"base",revision).commit()){
            loaded=false;conflict=true;state="disk_error";return;
        }
        loaded=true;baseline="";state="saved";
    }
    private static String request(int gen,String method,String data){return CzmAccount.call(gen,method,URL,"account",data==null?null:data.getBytes(StandardCharsets.UTF_8),"application/json");}
    private static void tick(){runSync(0);}
    private static void runSync(int choice){
        int id,gen;synchronized(CzmCloud.class){id=player;gen=generation;}
        try{sync(choice);}catch(Exception e){synchronized(CzmCloud.class){if(player==id&&generation==gen)state=e instanceof java.io.IOException?"disk_error":"retry";}}
    }
    // choice 1 loads cloud explicitly; 2 preserves this device explicitly on a conflict.
    private static void sync(int choice)throws Exception {
        final int id,gen;boolean fetch;int currentGeneration=CzmAccount.generation();
        synchronized(CzmCloud.class){id=player;gen=generation;if(id<=0||gen!=currentGeneration||applying||conflict&&choice==0)return;fetch=!loaded||choice!=0;}
        SharedPreferences pref=prefs();String root="p"+id+".";
        if(fetch){
            String response=request(gen,"GET",null);if(status(response)!=200){synchronized(CzmCloud.class){if(gen==generation)state="retry";}return;}
            JSONObject answer=body(response);JSONObject remote=answer.optJSONObject("payload");int rev=answer.getInt("revision");
            synchronized(CzmCloud.class){if(player!=id||generation!=gen)return;revision=rev;}
            String queued=pref.getString(root+"pending","");int base=pref.getInt(root+"base",0);
            if(choice==0&&!queued.isEmpty()&&base!=rev){synchronized(CzmCloud.class){if(player==id&&generation==gen){conflict=true;state="conflict";}}return;}
            if(choice==1 || choice==0&&queued.isEmpty()&&remote!=null){
                if(remote==null){synchronized(CzmCloud.class){if(player==id&&generation==gen){loaded=true;conflict=false;state="empty";}}return;}
                SettingsData.validate(remote);
                if(!pref.edit().putString(root+"backup",SettingsData.canonical(SettingsData.capture())).commit())throw new java.io.IOException("backup");
                String event=SettingsData.engineEvent(remote).toString();
                synchronized(CzmCloud.class){if(player!=id||generation!=gen)return;activeApply=++nextApply;if(activeApply<=0)activeApply=nextApply=1;pending=new JSONObject(event).put("cloudId",activeApply).toString();applying=true;conflict=false;state="restoring";}
                return;
            }
            synchronized(CzmCloud.class){if(player!=id||generation!=gen)return;loaded=true;conflict=false;baseline=pref.getString(root+"baseline","");}
        }
        JSONObject local=SettingsData.capture();String canonical=SettingsData.canonical(local);
        String queue=pref.getString(root+"pending","");
        synchronized(CzmCloud.class){
            if(player!=id||generation!=gen||applying)return;
            if(baseline.isEmpty() && revision>0 && queue.isEmpty() && choice==0){if(!pref.edit().putString(root+"baseline",canonical).commit())throw new java.io.IOException("baseline");baseline=canonical;return;}
            if(choice==0&&revision>0&&canonical.equals(baseline)&&queue.isEmpty())return;
        }
        if(!queue.isEmpty()&&choice==0)local=SettingsData.validate(new JSONObject(queue));
        canonical=SettingsData.canonical(local);
        int rev; synchronized(CzmCloud.class){if(player!=id||generation!=gen)return;rev=revision;state="saving";}
        if(!pref.edit().putString(root+"pending",canonical).putInt(root+"base",rev).commit())throw new java.io.IOException("queue");
        String response=request(gen,"POST",new JSONObject().put("revision",rev).put("payload",local).toString());
        synchronized(CzmCloud.class){
            if(player!=id||generation!=gen)return;
            if(status(response)==409){conflict=true;state="conflict";return;}
            if(status(response)!=200){state="retry";return;}
            revision=body(response).getInt("revision");baseline=canonical;
            state=pref.edit().remove(root+"pending").putInt(root+"base",revision).putString(root+"baseline",baseline).commit()?"saved":"disk_error";
        }
    }
    static void show(Activity a,boolean ru){
        String current; synchronized(CzmCloud.class){current=state;}
        CzmDialog d=new CzmDialog(a,ru?"Настройки в облаке":"Cloud settings",ru?"Кнопки, прицел, HUD, управление и звук":"Buttons, crosshair, HUD, controls and audio",ru?"Закрыть":"Close",ru?"Назад":"Back",ru,false);
        d.note(current.equals("conflict")?(ru?"На другом устройстве настройки изменились. Выбери, какую версию сохранить.":"Another device changed the settings. Choose which version to keep.")
            :current.equals("saved")?(ru?"Настройки сохранены в аккаунте.":"Settings are saved to your account.")
            :current.equals("apply_failed")?(ru?"Не удалось записать раскладку. Настройки устройства сохранены. Освободи место и повтори восстановление.":"Could not write the layout. Device settings were kept. Free some storage and restore again.")
            :current.equals("disk_error")?(ru?"Не удалось сохранить локальную копию. Освободи место и повтори действие.":"Could not save the local copy. Free some storage and retry.")
            :current.equals("retry")?(ru?"Ожидаем соединение. Изменения будут отправлены повторно.":"Waiting for a connection. Changes will be retried.")
            :(ru?"Синхронизация настроек…":"Syncing settings…"));
        d.link(ru?"Восстановить из облака":"Restore from cloud",v->choose(a,d,1,ru));
        d.link(ru?"Сохранить настройки этого устройства":"Save this device's settings",v->choose(a,d,2,ru));
        d.submit.setOnClickListener(v->d.dismiss());d.show();
    }
    private static void choose(Activity a,CzmDialog d,int choice,boolean ru){
        d.busy(true);WORK.execute(()->{runSync(choice);a.runOnUiThread(()->{if(a.isFinishing())return;d.dismiss();Toast.makeText(a,ru?"Запрос обработан. Статус доступен в аккаунте.":"Request processed. Check the account status.",Toast.LENGTH_SHORT).show();});});
    }
}
