package com.ffmpegtest;

import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.Surface;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    private VideoUtils player;
    private VideoView videoView;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.surfaceview);

        videoView = (VideoView) findViewById(R.id.video_view);
        player = new VideoUtils();
    }


    public void encode(View view){
        final String input = new File(Environment.getExternalStorageDirectory(),"love.flv").getAbsolutePath();
        final String output = new File(Environment.getExternalStorageDirectory(),"output_1280x720_yuv420p.yuv").getAbsolutePath();
        new Thread(new Runnable() {
            @Override
            public void run() {
                VideoUtils.decode(input, output);
            }
        }).start();
    }
    public void mPlay(View btn){
        String video = "love.flv";
        final String input = new File(Environment.getExternalStorageDirectory(),video).getAbsolutePath();
        //Surface传入到Native函数中，用于绘制
        final Surface surface = videoView.getHolder().getSurface();
        player.thread_play(input, surface);

      /*  String input = new File(Environment.getExternalStorageDirectory(),"input.flv").getAbsolutePath();
        String output = new File(Environment.getExternalStorageDirectory(),"wuchuanfang.pcm").getAbsolutePath();
        player.sound(input, output);*/
      //player.pthread();
    }
}
