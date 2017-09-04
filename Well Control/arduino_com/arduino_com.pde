import processing.serial.*;

import java.io.BufferedWriter;
import java.io.FileWriter;

Serial mySerial;
String outFilename = "out.txt";
String trialType = "";
String header;
String val;
boolean running = true;
boolean trialTypeSet = false;
boolean start = false;
void setup(){
  mySerial = new Serial(this, Serial.list()[1], 9600);
}
//"Date/Time, Target(mA), Return(mA), Flowrate(l/min)"
void draw(){
  if(mySerial.available() > 0){ 
    val = mySerial.readStringUntil('\n'); 
    if(val != null){
      val = val.trim();
      println(val);
      if(val.equals("Manual V")){
        trialType = "Manual V";
        header = "Date/Time, PWM (%), Well Current(mA), Flowrate(l/min)";
      }
      if(val.equals("Manual A")){
        trialType = "Manual A";
        header = "Date/Time, PWM (%), Well Current(mA), Flowrate(l/min)";
      }
      if(val.equals("Manual PWM")){
        trialType = "Manual PWM";
        header = "Date/Time, PWM (%), Well Current(mA), Flowrate(l/min)";
      }
      if(val.equals("Low High")){
        trialType = "Low High";
        header = "Date/Time, PWM (%), Actual Well Current(mA), Flowrate(l/min)";
      }
      if(val.equals("Proportional")){
        trialType = "Proportional";
        header = "Date/Time, PWM (%), Target Well Current(mA), Actual Well Current(mA), Flowrate(l/min)";
      }
      
      if(val.equals("Start of File") && start == false){
        int year = year(), month = month(), day = day(), hour = hour(), minute = minute(), second = second();
        String syear = nf(year, 4);
        String smonth = nf(month, 2);
        String sday = nf(day, 2);
        String shour = nf(hour, 2);
        String sminute = nf(minute, 2);
        String ssecond = nf(second, 2);
        outFilename = (trialType + "_" + syear + "-" + smonth + "-" + sday + " " + shour + "-" + sminute + "-" + ssecond) + ".txt";
        appendTextToFile(outFilename, header, true);
        start = true;
      }else if(val.equals("End of File") == false && start == true){ //list of ascii values that is valid for printing
        if(running){
          int year = year(), month = month(), day = day(), hour = hour(), minute = minute(), second = second();
          int millisecond = (int)(System.currentTimeMillis()%1000);
          String syear = nf(year, 4);
          String smonth = nf(month, 2);
          String sday = nf(day, 2);
          String shour = nf(hour, 2);
          String sminute = nf(minute, 2);
          String ssecond = nf(second, 2);
          String smillisecond = nf(millisecond, 3);
          appendTextToFile(outFilename, syear + "/" + smonth + "/" + sday + " " + shour + ":" + sminute + ":" + ssecond + "." + smillisecond + ", ", false);
          appendTextToFile(outFilename, val, true);
        }
      }else if(val.equals("End of File") && start == true){
        start = false;
      }
    }
  }
}

void appendTextToFile(String filename, String text, boolean enter){
  File f = new File(dataPath(filename));
  if(!f.exists()){
    createFile(f);
  }
  try{
    PrintWriter out = new PrintWriter(new BufferedWriter(new FileWriter(f, true)));
    if(enter){
      out.println(text);
    }else{ 
      out.print(text);
    }
    out.close();
  }catch(IOException e){
  }
}

void createFile(File f){
  File parentDir = f.getParentFile();
  try{
      parentDir.mkdirs();
      f.createNewFile();
  }catch(Exception e){
  }
}