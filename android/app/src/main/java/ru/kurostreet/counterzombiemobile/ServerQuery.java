/* Bounded A2S_INFO queries for the home widget. GPL-3.0-or-later. */
package ru.kurostreet.counterzombiemobile;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;

final class ServerQuery {
    static boolean valid(String s){if(s==null||!s.matches("(?:[0-9]{1,3}\\.){3}[0-9]{1,3}:[0-9]{1,5}"))return false;try{int port=Integer.parseInt(s.substring(s.lastIndexOf(':')+1));for(String octet:s.substring(0,s.indexOf(':')).split("\\."))if(Integer.parseInt(octet)>255)return false;return port>0&&port<=65535;}catch(Exception e){return false;}}
    static final class Info {String name,map;int players,max;}
    static String string(ByteArrayInputStream in)throws IOException{ByteArrayOutputStream out=new ByteArrayOutputStream();int b;while((b=in.read())>0){if(out.size()>512)throw new IOException();out.write(b);}if(b<0)throw new IOException();return new String(out.toByteArray(),StandardCharsets.UTF_8).replaceAll("[\\x00-\\x1f]","");}
    static Info parse(byte[] bytes,int length)throws IOException{
        if(length<6||length>bytes.length||bytes[0]!=-1||bytes[1]!=-1||bytes[2]!=-1||bytes[3]!=-1)throw new IOException();
        ByteArrayInputStream in=new ByteArrayInputStream(bytes,5,length-5);Info p=new Info();
        if(bytes[4]=='I'){in.read();p.name=string(in);p.map=string(in);string(in);string(in);in.read();in.read();p.players=in.read();p.max=in.read();}
        else if(bytes[4]=='m'){string(in);p.name=string(in);p.map=string(in);string(in);string(in);p.players=in.read();p.max=in.read();}
        else throw new IOException();if(p.players<0||p.max<0)throw new IOException();return p;
    }
    static Info query(String address)throws IOException{
        if(!valid(address))throw new IOException();int colon=address.lastIndexOf(':');
        InetSocketAddress target=new InetSocketAddress(InetAddress.getByName(address.substring(0,colon)),Integer.parseInt(address.substring(colon+1)));
        byte[] base="\u00ff\u00ff\u00ff\u00ffTSource Engine Query\0".getBytes(StandardCharsets.ISO_8859_1),message=base;
        try(DatagramSocket socket=new DatagramSocket()){socket.connect(target);socket.setSoTimeout(900);for(int n=0;n<2;n++){
            socket.send(new DatagramPacket(message,message.length));byte[] buffer=new byte[4096];DatagramPacket reply=new DatagramPacket(buffer,buffer.length);socket.receive(reply);
            if(reply.getLength()==9&&buffer[0]==-1&&buffer[1]==-1&&buffer[2]==-1&&buffer[3]==-1&&buffer[4]=='A'){message=new byte[base.length+4];System.arraycopy(base,0,message,0,base.length);System.arraycopy(buffer,5,message,base.length,4);continue;}
            return parse(buffer,reply.getLength());
        }}throw new IOException();
    }
}
