package ru.kurostreet.counterzombiemobile;
public final class DownloadsStateTest {
 static void check(boolean b){if(!b)throw new AssertionError();}
 public static void main(String[] args){
  CzmDownloads.progress(1,0,34,"Server / map");check(CzmDownloads.snapshot().percent==34);
  CzmDownloads.cancel(99);check(!CzmDownloads.cancelEvent());
  CzmDownloads.cancel(1);check(CzmDownloads.cancelEvent()&&!CzmDownloads.cancelEvent());
  CzmDownloads.progress(1,3,100,"ready");check(CzmDownloads.snapshot().status==3);
  CzmDownloads.progress(2,0,-1,"next");check(CzmDownloads.snapshot().session==2&&CzmDownloads.snapshot().percent==0&&!CzmDownloads.cancelEvent());
  CzmDownloads.cancel(1);check(!CzmDownloads.cancelEvent());
  CzmDownloads.progress(0,0,50,"invalid");check(CzmDownloads.snapshot().session==2);
  CzmDownloads.progress(2,1,140,"done");check(CzmDownloads.snapshot().percent==100&&CzmDownloads.snapshot().status==1);
  System.out.println("DOWNLOADS_STATE_OK");
 }
}
