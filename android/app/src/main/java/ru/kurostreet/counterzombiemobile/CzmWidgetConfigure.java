package ru.kurostreet.counterzombiemobile;
import android.app.*;
import android.appwidget.AppWidgetManager;
import android.content.*;
import android.os.Bundle;
import java.util.*;

public final class CzmWidgetConfigure extends Activity {
    @Override public void onCreate(Bundle state){super.onCreate(state);setResult(RESULT_CANCELED);
        int id=getIntent().getIntExtra(AppWidgetManager.EXTRA_APPWIDGET_ID,AppWidgetManager.INVALID_APPWIDGET_ID);if(id==AppWidgetManager.INVALID_APPWIDGET_ID){finish();return;}
        boolean ru=Locale.getDefault().getLanguage().equals("ru");CzmDialog d=new CzmDialog(this,ru?"Серверы CZM":"CZM servers",ru?"Выбери серверы с сердечком":"Choose servers marked with a heart",ru?"Все сердечки":"All hearts",ru?"Отмена":"Cancel",ru,false);
        List<String[]> list=CzmServerWidget.hearts(this);for(String[] item:list){final String address=item[0];d.link(address,v->save(id,address));}d.submit.setOnClickListener(v->save(id,""));d.cancel.setOnClickListener(v->finish());d.setOnCancelListener(v->finish());d.show();
    }
    private void save(int id,String choice){getSharedPreferences("czm_widget",0).edit().putString("choice"+id,choice).apply();sendBroadcast(new Intent(this,CzmServerWidget.class).setAction(CzmServerWidget.REFRESH).putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS,new int[]{id}));setResult(RESULT_OK,new Intent().putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID,id));finish();}
}
