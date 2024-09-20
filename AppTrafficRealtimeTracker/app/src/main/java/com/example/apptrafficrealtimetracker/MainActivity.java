package com.example.apptrafficrealtimetracker;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.app.AppOpsManager;
import android.app.usage.NetworkStats;
import android.content.pm.ApplicationInfo;
import android.net.ConnectivityManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Message;
import android.os.Handler;
import android.os.Looper;
import android.app.Activity;
import android.net.TrafficStats;
import android.net.wifi.WifiManager;
import android.app.usage.NetworkStatsManager;
import android.content.Context;
import android.content.Intent;

import android.provider.Settings;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;
import android.widget.Button;
import android.widget.Toast;
import android.util.Log;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.text.DecimalFormat;
import java.util.Calendar;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class MainActivity extends AppCompatActivity {
    DecimalFormat df = new DecimalFormat(".##");
    private TextView logtextview;
    private Handler m_handler;
    private Button btn_start_log, btn_stop_log;
    private NetworkStatsManager nsm;
    private WifiManager wifiManager;
    public static int wifiState;

    private Map<Integer, AppStatus> appStats;

    private static File sdCardFile, file;


    private double prevTime = -1;
    private double prevRxWifiTraffic = -1;
    private double prevTxWifiTraffic = -1;
    public static String wifiRxRateAllApp;
    public static String wifiTxRateAllApp;

    private final int TWO_HOURS_AS_MS = 3600000 * 2;


    @RequiresApi(api = Build.VERSION_CODES.M)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (!isAccessGranted()) {
            Intent intent = new Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS);
            startActivity(intent);
        }

        setContentView(R.layout.activity_main);

        wifiManager = (WifiManager)getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        wifiState = wifiManager.getWifiState();

        logtextview = (TextView) findViewById(R.id.log_region);

        btn_start_log = (Button) findViewById(R.id.btn_start);
        btn_start_log.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                m_handler.post(PeriodicalCollect);
            }
        });

        btn_stop_log = (Button) findViewById(R.id.btn_stop);
        btn_stop_log.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                m_handler.removeCallbacks(PeriodicalCollect);
            }
        });

        m_handler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(@NonNull Message msg){
                super.handleMessage(msg);
                if(msg.what == 1){
                    logtextview.setText("Time " + df.format(System.currentTimeMillis()));
                }
            }
        };

        try {

            PackageManager manager = getPackageManager();
//            PackageInfo packageInfo = manager.getPackageInfo(getPackageName(), 0);
            List<PackageInfo> packages = manager.getInstalledPackages(0);
            appStats = new HashMap<Integer, AppStatus>();
            for (PackageInfo packageInfo : packages) {
                ApplicationInfo appInfo = packageInfo.applicationInfo;
                String appName = appInfo.loadLabel(manager).toString();
                int uid = appInfo.uid;
                AppStatus appstat = new AppStatus();
                appstat.appName = appName;
                appstat.uid = uid;
                long init_time = 0;
                appstat.prevTimeByApp = init_time;
                appStats.put(uid, appstat);
//                logtextview.append("appName" + appName);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }


        nsm = (NetworkStatsManager) getApplicationContext().getSystemService(NETWORK_STATS_SERVICE);

        sdCardFile = Environment.getExternalStorageDirectory();
//        File directory_tracker = new File(sdCardFile, "TrafficTracker");
//        if (!directory_tracker.exists()){
//            directory_tracker.mkdirs();
//        }

        File directory_doc = new File(sdCardFile, "Documents");
//        String path = getApplicationContext().getFilesDir().getPath();

        String filename = "apptraffic_" + df.format(System.currentTimeMillis())+".txt";
        file = new File(directory_doc, filename);
        try {
            if (file.createNewFile()) {//创建文件成功
                logtextview.append("CreateFile: " + filename);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

        saveUserInfo(getApplicationContext(), file, "Start", " ", " ", " ");
    }

    public class AppStatus {
        public Integer uid;
        public String appName;
        public Double prevRxAppTraffic = -1.0;
        public Double prevTxAppTraffic = -1.0;
        public Long prevTimeByApp;
    }


    private boolean isAccessGranted() {
        try {
            PackageManager packageManager = getPackageManager();
            ApplicationInfo applicationInfo = packageManager.getApplicationInfo(getPackageName(), 0);
            AppOpsManager appOpsManager = (AppOpsManager) getSystemService(Context.APP_OPS_SERVICE);
            int mode = 0;
            if (android.os.Build.VERSION.SDK_INT > android.os.Build.VERSION_CODES.KITKAT) {
                mode = appOpsManager.checkOpNoThrow(AppOpsManager.OPSTR_GET_USAGE_STATS,
                        applicationInfo.uid, applicationInfo.packageName);
            }
            return (mode == AppOpsManager.MODE_ALLOWED);

        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }



    public double getRxWifiTraffic() {
        double rTotal = TrafficStats.getTotalRxBytes();
        double rGprs = TrafficStats.getMobileRxBytes();
        double rWifi = rTotal - rGprs;
        return rWifi;
    }

    public double getTxWifiTraffic() {
        double tTotal = TrafficStats.getTotalTxBytes();
        double tGprs = TrafficStats.getMobileTxBytes();
        double tWifi = tTotal - tGprs;
        return tWifi;
    }

    private Runnable PeriodicalCollect = new Runnable () {

        @RequiresApi(api = Build.VERSION_CODES.M)
        @Override
        public void run(){
            if (prevTime < 0 || prevRxWifiTraffic < 0 || prevTxWifiTraffic < 0) {
                prevTime = System.currentTimeMillis();
                prevRxWifiTraffic = getRxWifiTraffic();
                prevTxWifiTraffic = getTxWifiTraffic();
            }
            else {
                double currentTime = System.currentTimeMillis();
                double currentRxWifiTraffic = getRxWifiTraffic();
                double currentTxWifiTraffic = getTxWifiTraffic();
                double deltaTime = (currentTime - prevTime);
                if (deltaTime < 5) deltaTime = 5;
                double deltaRxWifiTraffic = currentRxWifiTraffic - prevRxWifiTraffic;
                double deltaTxWifiTraffic = currentTxWifiTraffic - prevTxWifiTraffic;
                double transientRxWifiRate = deltaRxWifiTraffic * 8 / deltaTime; // in kbps
                double transientTxWifiRate = deltaTxWifiTraffic * 8 / deltaTime; // in kbps
                wifiRxRateAllApp = df.format(transientRxWifiRate);
                wifiTxRateAllApp = df.format(transientTxWifiRate);

//                Log.i("PrevTime", df.format(prevTime));
//                Log.i("CurrentTime", df.format(currentTime));
//                Log.i("WifiRateAllApp", wifiRateAllApp);
                logtextview.setText("");
                logtextview.append("WifiRxRateAllApp(kbps) " + wifiRxRateAllApp + " WifiTxRateAllApp(kbps) " + wifiTxRateAllApp+"\n");

                prevTime = currentTime;
                prevRxWifiTraffic = currentRxWifiTraffic;
                prevTxWifiTraffic = currentTxWifiTraffic;
            }

            for (Map.Entry<Integer, AppStatus> app_entry : appStats.entrySet()) {
                int uid = app_entry.getKey();
                AppStatus appstat = app_entry.getValue();


                long start = 0;
                long currentTime = System.currentTimeMillis();
//                double currentRxAppTraffic = TrafficStats.getUidRxBytes(uid);
//                double currentTxAppTraffic = TrafficStats.getUidTxBytes(uid);
                double currentRxAppTraffic = 0;
                double currentTxAppTraffic = 0;

                NetworkStats appWifiUsage = nsm.queryDetailsForUid(ConnectivityManager.TYPE_WIFI, null, start, (long)currentTime + TWO_HOURS_AS_MS, uid);
//                    NetworkStats appWifiUsage = nsm.queryDetailsForUid(ConnectivityManager.TYPE_WIFI, null, (long)prevTimeByApp.get(i) - 864000, (long)currentTime + 86400000, uid);
                do {
                    NetworkStats.Bucket bucket = new NetworkStats.Bucket();
                    appWifiUsage.getNextBucket(bucket);
                    currentRxAppTraffic += bucket.getRxBytes();
                    currentTxAppTraffic += bucket.getTxBytes();
                } while (appWifiUsage.hasNextBucket());


                if (appstat.prevRxAppTraffic < 0 || appstat.prevTxAppTraffic < 0 || appstat.prevTimeByApp < 0){
                    appstat.prevRxAppTraffic = currentRxAppTraffic;
                    appstat.prevTxAppTraffic = currentTxAppTraffic;
                    appstat.prevTimeByApp = currentTime;
                }
                else{
//                    if (i > 20) logtextview.append(appNames.get(i) + " Rx " + currentRxAppTraffic);
                    double deltaTime = (currentTime - appstat.prevTimeByApp);
                    if (deltaTime < 500) deltaTime = 500;
                    double deltaRxAppTraffic = currentRxAppTraffic - appstat.prevRxAppTraffic;
                    double deltaTxAppTraffic = currentTxAppTraffic - appstat.prevTxAppTraffic;


//                    double transientRxAppRate = deltaRxAppTraffic * 8 / deltaTime; // in kbps
//                    double transientTxAppRate = deltaTxAppTraffic * 8 / deltaTime; // in kbps

                    double transientRxAppRate = deltaRxAppTraffic * 8 / deltaTime; // in kbps
                    double transientTxAppRate = deltaTxAppTraffic * 8 / deltaTime; // in kbps
                    if (transientRxAppRate > 0 || transientTxAppRate > 0){
                        // log
                        String appRxRate = df.format(transientRxAppRate);
                        String appTxRate = df.format(transientTxAppRate);

//                        Log.i("PrevTime", df.format(prevTime));
//                        Log.i("CurrentTime", df.format(currentTime));
//                        Log.i("AppName", appNames.get(i));
//                        Log.i("AppRxRate", appRxRate);
//                        Log.i("AppTxRate", appTxRate);

                        logtextview.append(appstat.appName + " RxRate(kbps) " + appRxRate + " TxRate(kbps) " + appTxRate + "\n");
                        checkWriteAndReadPermission();
                        saveUserInfo(getApplicationContext(), file, df.format(currentTime), df.format(appstat.uid), appRxRate, appTxRate);

                    }
//                    if(currentRxAppTraffic > 0 || currentTxAppTraffic > 0)
//                        logtextview.append(appstat.appName + " TotalRxRate(kbps) " + df.format(currentRxAppTraffic) + " TotalTxRate(kbps) " + df.format(currentTxAppTraffic) + "\n");
                    appstat.prevRxAppTraffic = currentRxAppTraffic;
                    appstat.prevTxAppTraffic = currentTxAppTraffic;
                    appstat.prevTimeByApp = currentTime;
                }
            }

            m_handler.postDelayed(PeriodicalCollect, 120000); // get statistics for every 30min
            }
    };

    public static boolean saveUserInfo(Context context, File file, String time, String uid, String rxRate, String txRate) {

        try {
            // 判断当前的手机是否有sd卡
            String state = Environment.getExternalStorageState();

            if (!Environment.MEDIA_MOUNTED.equals(state)) {
                // 已经挂载了sd卡
                return false;
            }


            FileOutputStream fos = new FileOutputStream(file, true);
            String data = time + " " + uid + " " + rxRate + " " + txRate + "\r\n";
            fos.write(data.getBytes());
            fos.flush();
            fos.close();

//            FileWriter fw = new FileWriter(file);
//            fw.write(data);
//            fw.close();

            return true;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    };

    private void checkWriteAndReadPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED
                    || checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED) {
                String[] permissions = new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE};
                requestPermissions(permissions, 1000);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        for (int permission : grantResults) {
            if (permission == PackageManager.PERMISSION_DENIED) {
                Toast.makeText(this, "Fail to get permission", Toast.LENGTH_LONG).show();
                break;
            }
        }
    }




}