package com.example.taskmon;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.jjoe64.graphview.GraphView;
import com.jjoe64.graphview.GraphView.GraphViewData;
import com.jjoe64.graphview.GraphView.LegendAlign;
import com.jjoe64.graphview.GraphViewDataInterface;
import com.jjoe64.graphview.GraphViewSeries;
import com.jjoe64.graphview.GraphViewSeries.GraphViewSeriesStyle;
import com.jjoe64.graphview.GraphViewStyle.GridStyle;
import com.jjoe64.graphview.LineGraphView;
import com.jjoe64.graphview.ValueDependentColor;

import android.support.v7.app.ActionBarActivity;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TableLayout;
import android.widget.TableRow.LayoutParams;
import android.widget.TableRow;
import android.widget.TextView;

public class MainActivity extends ActionBarActivity implements Runnable{
	
	private GraphView graphView;
	private GraphViewSeriesStyle seriesStyle = new GraphViewSeriesStyle();
//	Map<Integer, GraphViewSeries> threadDataMap = new HashMap<Integer, GraphViewSeries>();
//	Map<Integer, Integer> lastValue = new HashMap<Integer, Integer>();
	private Handler mHandler = new Handler();
	private boolean updateGraph = false;
//	private int[] realTidArray;
	private int[] reserveTidArray;
	private LinearLayout layout;
//	private boolean justOpened;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
//        addItmesOnThreadID();
        addItmesOnReserveID();
		updateFreq();
		updatePower();
		updateEnergy();
        
//        seriesStyle.setValueDependentColor(new ValueDependentColor() {
//    	  @Override
//    	  public int get(GraphViewDataInterface data) {
//    	    // the higher the more red
//        	    return Color.rgb((int)(150+((data.getY()/3)*100)), (int)(150-((data.getY()/3)*150)), (int)(150-((data.getY()/3)*150)));
//    	  }
//    	});
        
//        createGraph();

    	mHandler.postDelayed(this, 1000);
	}
	
	private void createGraph(){
        GraphView graphView = new LineGraphView(this, "Utilization");
        
        this.graphView = graphView;
    	this.graphView.setViewPort(0, 10000);
    	this.graphView.setScrollable(true);
    	this.graphView.setScalable(true);
    	this.graphView.setLegendAlign(LegendAlign.TOP);
    	this.graphView.setShowLegend(true);
    	this.graphView.setManualYAxisBounds(1,0);
    	this.graphView.getGraphViewStyle().setNumVerticalLabels(6);
    	this.graphView.getGraphViewStyle().setNumHorizontalLabels(10);
    	this.graphView.getGraphViewStyle().setGridStyle(GridStyle.HORIZONTAL);
    	
    	layout = (LinearLayout) findViewById(R.id.graph);
    	layout.addView(graphView);
	}
	
	public void run(){
//		addItmesOnThreadID();
		addItmesOnReserveID();
		updateFreq();
		updatePower();
		updateEnergy();
		
		TableLayout table = (TableLayout) findViewById(R.id.tidpower);
		table.removeAllViews();
		
		TableRow title_row = new TableRow(this);
		title_row.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT,LayoutParams.WRAP_CONTENT));
		
		TextView title = new TextView(this);
		title.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT,LayoutParams.WRAP_CONTENT));
		title.setPadding(5, 5, 5, 5);
		title.setText("Threads Energy List");
		
		title_row.addView(title);
		table.addView(title_row);
		

		if(reserveTidArray != null){
			int n = reserveTidArray.length;
			int unusedcount = 0;
			
			int count = 1;
			for(int i = 1; i < reserveTidArray.length; i++){
				if (reserveTidArray[i] > 100000 || reserveTidArray[i] == 0){
					continue;
				}
				
				int tidEnergy = JniWrapper.getTidEnergy(reserveTidArray[i]);
				
				if (count < table.getChildCount()){
					TableRow row = (TableRow)table.getChildAt(count);
					((TextView)row.getChildAt(0)).setText("tid: " + reserveTidArray[i]);
					((TextView)row.getChildAt(1)).setText("Energy: " + tidEnergy);
					
				}else{
					 TableRow row = new TableRow(this);
					 row.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT,LayoutParams.WRAP_CONTENT));
					 
					 TextView row_name = new TextView(this);
					 row_name.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT,LayoutParams.WRAP_CONTENT));
					 row_name.setPadding(5, 5, 5, 5);
					 row_name.setText("tid: " + reserveTidArray[i]);
					 
					 TextView row_value = new TextView(this);
					 row_value.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT,LayoutParams.WRAP_CONTENT));
					 row_value.setPadding(5, 5, 5, 5);
					 row_value.setText("Energy: "+ tidEnergy);
					 
					 
					 row.addView(row_name);
					 row.addView(row_value);
					 
					 table.addView(row);
				}
				
				count++;
			}
			
		}
		
		table.invalidate();
		table.refreshDrawableState();
		
		mHandler.postDelayed(this, 1000);
		
	}


	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		// Handle action bar item clicks here. The action bar will
		// automatically handle clicks on the Home/Up button, so long
		// as you specify a parent activity in AndroidManifest.xml.
		int id = item.getItemId();
		if (id == R.id.action_settings) {
			return true;
		}
		return super.onOptionsItemSelected(item);
	}
	
	
//	public void startMonitor(View view){
//		TextView response = (TextView) findViewById(R.id.StartMonitorResponse);
//		int ret = JniWrapper.taskmonEnable();
//		
//		if(ret == 0){
//			response.setText("Success");
//			justOpened = true;
//			updateGraph = true;
//			return;
//		}
//	
//		response.setText("fail" + ret);
//		
//		
//	}
	
	public void stopMonitor(View view){
		TextView response = (TextView) findViewById(R.id.StopMonitorResponse);
		if(JniWrapper.taskmonDisable() == 0){
			response.setText("Success");
			updateGraph = false;
			return;
		}
		response.setText("fail");
	}
	
//	private void addItmesOnThreadID(){
//
//		Spinner threadIDSpin = (Spinner) findViewById(R.id.relThreadID);
//		int[] tidArray = JniWrapper.getRelTID();
//		List<Integer> tidlist = new ArrayList<Integer>();
//		
//		if(tidArray == null){
//			return;
//		}
//		
//		if(realTidArray != null && Arrays.equals(realTidArray, tidArray)){
//			return;
//		}
//		realTidArray = tidArray;
//		
//		for(int i = 0; i < tidArray.length; i++ ){
//			tidlist.add(tidArray[i]);
//		}
//		ArrayAdapter<Integer> dataAdapter = new ArrayAdapter<Integer>(this,
//				android.R.layout.simple_spinner_item, tidlist);
//		dataAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
//		threadIDSpin.setAdapter(dataAdapter);
//	}
	
	private void addItmesOnReserveID(){

//		Spinner threadIDSpin = (Spinner) findViewById(R.id.cancelThreadID);
		int[] resTidArray = JniWrapper.getReservedTID();
		List<Integer> tidlist = new ArrayList<Integer>();
		
		if(resTidArray == null){
			return;
		}
		
		if(reserveTidArray != null && Arrays.equals(reserveTidArray, resTidArray)){
			return;
		}
		
		reserveTidArray = resTidArray;
		
//		for(int i = 0; i < reserveTidArray.length; i++ ){
//			if(reserveTidArray[i] > 0 && reserveTidArray[i] < 100000){
//				tidlist.add(reserveTidArray[i]);
//			}
//		}
//		ArrayAdapter<Integer> dataAdapter = new ArrayAdapter<Integer>(this,
//				android.R.layout.simple_spinner_item, tidlist);
//		dataAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
//		threadIDSpin.setAdapter(dataAdapter);
	}
	
	private void updateFreq(){

		TextView freqTextview = (TextView) findViewById(R.id.freq);
		int freq = JniWrapper.getFreq();
		
		if(freq < 0){
			if(freq == -1){
				freqTextview.setText("parse error");
			}else if(freq == -2){
				freqTextview.setText("read error");
			}else if(freq == -3){
				freqTextview.setText("open error");
			}
			return;
		}
		
		freqTextview.setText("freq: " + freq);
	}
	
	private void updatePower(){

		TextView powerTextview = (TextView) findViewById(R.id.power);
		int power = JniWrapper.getPower();
		
		if(power < 0){
			if(power == -1){
				powerTextview.setText("parse error");
			}else if(power == -2){
				powerTextview.setText("read error");
			}else if(power == -3){
				powerTextview.setText("open error");
			}
			
			return;
		}
		
		powerTextview.setText("Power: " + power);
	}
	
	private void updateEnergy(){

		TextView energyTextview = (TextView) findViewById(R.id.energy);
		int energy = JniWrapper.getEnergy();
		
		if(energy < 0){
			if(energy == -1){
				energyTextview.setText("parse error");
			}else if(energy == -2){
				energyTextview.setText("read error");
			}else if(energy == -3){
				energyTextview.setText("open error");
			}
			return;
		}
		
		energyTextview.setText("Energy: " + energy);
	}
}
