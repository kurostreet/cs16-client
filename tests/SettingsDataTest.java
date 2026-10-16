package ru.kurostreet.counterzombiemobile;
import org.json.*;
import java.util.*;

public final class SettingsDataTest {
    static void check(boolean b){if(!b)throw new AssertionError();}
    static JSONObject payload()throws Exception{
        return new JSONObject("{\"schema\":1,\"cvars\":{\"czm_hud_style\":\"2\",\"czm_touch_scheme\":\"1\"},\"layouts\":{\"1\":[[\"attack\",\"touch/fire\",\"+attack\",0.8,0.6,0.95,0.9,255,255,255,170,0]]}}");
    }
    public static void main(String[] args)throws Exception{
        JSONObject p=payload();SettingsData.validate(p);JSONObject event=SettingsData.engineEvent(p);String cfg=event.getJSONObject("files").getString("1");check(cfg.startsWith("touch_removeall\n")&&cfg.contains("\"+attack\"")&&!cfg.contains("password"));
        check(SettingsData.tokens("touch_addbutton \"attack\" \"touch/fire\" \"say hello world\" 0 0 1 1").get(3).equals("say hello world"));
        for(String cmd:new String[]{"exec stolen.cfg","+attack;quit","say ok\nquit","connect 1.1.1.1"}){p=payload();p.getJSONObject("layouts").getJSONArray("1").getJSONArray(0).put(2,cmd);try{SettingsData.validate(p);throw new AssertionError(cmd);}catch(java.io.IOException expected){}}
        p=payload();p.getJSONObject("cvars").put("password","secret");try{SettingsData.validate(p);throw new AssertionError();}catch(java.io.IOException expected){}
        p=payload();p.getJSONObject("layouts").getJSONArray("1").getJSONArray(0).put(1,"../config.cfg");try{SettingsData.validate(p);throw new AssertionError();}catch(java.io.IOException expected){}
        for(String bad:new String[]{"NaN","Infinity","1;quit","1\nquit"})check(!SettingsData.validValue("volume",bad));
        p=payload();String canonical=SettingsData.canonical(p);check(SettingsData.canonical(new JSONObject(canonical)).equals(canonical));
        System.out.println("SETTINGS_DATA_OK");
    }
}
