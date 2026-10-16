package ru.kurostreet.counterzombiemobile;
import java.net.*;
import java.io.*;
import java.util.Arrays;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;

/** Real UDP exchange; neither a live server nor the game is started. */
public final class ServerQueryTest {
    static void check(boolean b){if(!b)throw new AssertionError();}
    static byte[] modern()throws Exception{
        ByteArrayOutputStream out=new ByteArrayOutputStream();out.write(new byte[]{-1,-1,-1,-1,'I',48});
        out.write("CZM test\0zm_lab\0cstrike\0CZM\0".getBytes(StandardCharsets.UTF_8));out.write(new byte[]{10,0,12,32,0,'d','l',0,0});return out.toByteArray();
    }
    public static void main(String[] args)throws Exception{
        for(String bad:new String[]{"a:27015","256.1.1.1:27015","1.1.1.1:0","1.1.1.1:65536","1.1.1.1:27015;quit",null})check(!ServerQuery.valid(bad));
        check(ServerQuery.valid("127.0.0.1:27015"));byte[] response=modern();ServerQuery.Info info=ServerQuery.parse(response,response.length);check(info.players==12&&info.max==32&&info.name.equals("CZM test")&&info.map.equals("zm_lab"));
        for(int n:new int[]{0,5,10,response.length+1}){try{ServerQuery.parse(response,n);throw new AssertionError();}catch(IOException expected){}}
        byte[] legacy=new byte[]{-1,-1,-1,-1,'m'};ByteArrayOutputStream old=new ByteArrayOutputStream();old.write(legacy);old.write("127.0.0.1:27015\0Legacy\0zm_old\0cstrike\0Game\0".getBytes(StandardCharsets.UTF_8));old.write(new byte[]{3,16});byte[] packet=old.toByteArray();info=ServerQuery.parse(packet,packet.length);check(info.players==3&&info.max==16&&info.map.equals("zm_old"));
        ExecutorService worker=Executors.newSingleThreadExecutor();
        try(DatagramSocket server=new DatagramSocket(0,InetAddress.getByName("127.0.0.1"))){
            server.setSoTimeout(2000);
            Future<?> peer=worker.submit(()->{try{
                DatagramPacket request=new DatagramPacket(new byte[4096],4096);server.receive(request);int length=request.getLength();
                byte[] challenge={-1,-1,-1,-1,'A',3,7,11,13};server.send(new DatagramPacket(challenge,challenge.length,request.getSocketAddress()));
                request.setLength(4096);server.receive(request);check(request.getLength()==length+4);check(Arrays.equals(Arrays.copyOfRange(request.getData(),length,length+4),new byte[]{3,7,11,13}));server.send(new DatagramPacket(response,response.length,request.getSocketAddress()));
            }catch(Exception e){throw new RuntimeException(e);}});
            info=ServerQuery.query("127.0.0.1:"+server.getLocalPort());check(info.players==12);peer.get(3,TimeUnit.SECONDS);
        }finally{worker.shutdownNow();}
        System.out.println("SERVER_QUERY_OK");
    }
}
