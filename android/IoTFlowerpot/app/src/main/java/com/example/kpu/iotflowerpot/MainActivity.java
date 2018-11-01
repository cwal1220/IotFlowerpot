package com.example.kpu.iotflowerpot;

import android.app.DatePickerDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.graphics.Color;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.DatePicker;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.github.mikephil.charting.charts.LineChart;
import com.github.mikephil.charting.components.AxisBase;
import com.github.mikephil.charting.data.Entry;
import com.github.mikephil.charting.data.LineData;
import com.github.mikephil.charting.data.LineDataSet;
import com.github.mikephil.charting.formatter.IAxisValueFormatter;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class MainActivity extends AppCompatActivity implements DBRequester.Listener{

    Button button_search, button_date, button_water, button_deviceId;
    TextView textView_date;
    String datetime;
    DatePickerDialog datePickerDialog;
    // LineChart
    LineChart lineChart;

    private ProgressDialog progressDialog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Date current_date = new Date(System.currentTimeMillis());
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd", Locale.KOREA);
        datetime = sdf.format(current_date);

        // Widget Init
        button_water        = findViewById(R.id.button_water);
        button_search       = findViewById(R.id.button_search);
        button_date         = findViewById(R.id.button_date);
        button_deviceId     = findViewById(R.id.button_deviceId);
        textView_date       = findViewById(R.id.textView_date);
        textView_date.setText(datetime);

        // Progress Bar
        progressDialog = new ProgressDialog(MainActivity.this);
        progressDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        progressDialog.setMessage("검색중입니다...");


        // LineChart Init
        lineChart = findViewById(R.id.lineChart);


        // Make DatePickerDialog
        DatePickerDialog.OnDateSetListener onDateSetListener = new DatePickerDialog.OnDateSetListener() {
            @Override
            public void onDateSet(DatePicker view, int year, int month, int dayOfMonth) {
                datetime = year + "-" + (month+1) + "-" + dayOfMonth;
                textView_date.setText(datetime);
            }
        };
        datePickerDialog = new DatePickerDialog(this, onDateSetListener, current_date.getYear()+1900, current_date.getMonth(), current_date.getDay());


        button_search.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                try {
                    JSONObject param = new JSONObject();
                    param.put("device_id", button_deviceId.getText().toString());
                    param.put("start_time", textView_date.getText() + " 00:00:00");
                    param.put("end_time", textView_date.getText() + " 23:59:59");
                    new DBRequester.Builder(MainActivity.this, "http://52.78.239.38:5000", MainActivity.this)
                            .attach("request/record")
                            .streamPost(param)
                            .request("request record");
                    progressDialog.show();

                } catch (JSONException e) {
                    e.printStackTrace();
                }
            }
        });


        button_date.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                datePickerDialog.show();
            }
        });


        button_deviceId.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AlertDialog.Builder ad = new AlertDialog.Builder(MainActivity.this);
                final EditText et = new EditText(MainActivity.this);
                et.setText(button_deviceId.getText());
                ad.setView(et);
                ad.setTitle("Device ID");       // 제목 설정
                ad.setMessage("장치의 ID를 입력하세요");   // 내용 설정

                ad.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        button_deviceId.setText(et.getText());
                    }
                });
                ad.show();
            }
        });

        button_water.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                try {
                    JSONObject param = new JSONObject();
                    param.put("device_id", button_deviceId.getText().toString());
                    new DBRequester.Builder(MainActivity.this, "http://52.78.239.38:5000", MainActivity.this)
                            .attach("request/led")
                            .streamPost(param)
                            .request("request led");
                    progressDialog.show();
                } catch (JSONException e) {
                    e.printStackTrace();
                }
            }
        });

    }

    @Override
    public void onResponse(String id, JSONObject json, Object... params) {
        try {
            if (json.getBoolean("success") == false) {
                Toast.makeText(this, "요청 실패", Toast.LENGTH_SHORT).show();
                return;
            }
            switch (id) {
                case "request led":
                    progressDialog.dismiss();
                    Toast.makeText(this, "물주기 요청을 보냈습니다. \n 최대 1분정도 걸릴 수 있습니다.", Toast.LENGTH_LONG).show();
                    break;

                case "request record":
                    List<Entry> temperature_entries;
                    List<Entry> humidity_entries;
                    List<Entry> dirt_entries;

                    temperature_entries = new ArrayList<>();
                    humidity_entries = new ArrayList<>();
                    dirt_entries = new ArrayList<>();
                    // Clear DataSet
                    lineChart.clear();



                    JSONArray jsonArray = json.getJSONObject("data").getJSONArray("data");
                    for (int i = 0; i < jsonArray.length(); i++) {
                        int timestamp = jsonArray.getJSONObject(i).getInt("timestamp") - 1140000 ;
                        dirt_entries.add(new Entry(timestamp , (float) jsonArray.getJSONObject(i).getDouble("dirt")));
                        temperature_entries.add(new Entry(timestamp, (float) jsonArray.getJSONObject(i).getDouble("temperature")));
                        humidity_entries.add(new Entry(timestamp, (float) jsonArray.getJSONObject(i).getDouble("humidity")));
                        dirt_entries.add(new Entry(timestamp, (float) jsonArray.getJSONObject(i).getDouble("dirt")));


                    }
                    if(jsonArray.length() > 1) {
                        LineDataSet lineDataSet_temp = new LineDataSet(temperature_entries, "온도(℃)");
                        LineDataSet lineDataSet_humi = new LineDataSet(humidity_entries, "습도(%)");
                        LineDataSet lineDataSet_dirt = new LineDataSet(dirt_entries, "토양(%)");


                        LineData lineData = new LineData(lineDataSet_temp);
                        lineData.addDataSet(lineDataSet_humi);
                        lineData.addDataSet(lineDataSet_dirt);
                        lineDataSet_temp.setColor(Color.RED);
                        lineDataSet_temp.setCircleColor(Color.RED);
                        lineDataSet_temp.setDrawCircles(false);
                        lineDataSet_humi.setColor(Color.BLUE);
                        lineDataSet_humi.setCircleColor(Color.BLUE);
                        lineDataSet_humi.setDrawCircles(false);
                        lineDataSet_dirt.setColor(Color.GREEN);
                        lineDataSet_dirt.setCircleColor(Color.GREEN);
                        lineDataSet_dirt.setDrawCircles(false);
                        lineChart.getXAxis().setValueFormatter(new IAxisValueFormatter() {

                            private SimpleDateFormat mFormat = new SimpleDateFormat("HH:mm:ss");

                            @Override
                            public String getFormattedValue(float value, AxisBase axis) {
                                return mFormat.format(value);
                            }
                        });
                        lineChart.setData(lineData);
                    }
                    lineChart.invalidate();
                    lineChart.getDescription().setText("측정값");
                    progressDialog.dismiss();
            }

        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onResponse(String id, JSONArray json, Object... params) {

    }

    @Override
    public void onError(String id, String message, Object... params) {

    }
}
