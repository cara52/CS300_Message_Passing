package edu.cs300;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.util.Enumeration;
import java.util.ArrayList;
import java.util.Scanner;
import java.util.Vector;

public class ReportingSystem extends Thread{


        private int reportIdx;
        private int reportCount;
        private File inputFile;

        public void run() {
           try {
              // creates scanner for reading files
              Scanner inReportList = new Scanner(inputFile);
              String reportTitle=inReportList.nextLine();
              String searchString=inReportList.nextLine();
              // creates output file based on info provided
              String out=inReportList.nextLine();
              File outputFile = new File(out);
              outputFile.createNewFile();
              FileWriter writer = new FileWriter(outputFile);
              writer.write(reportTitle+"\n");

              // arrays containing info for the headers and start and end
              // indicies for strings
              ArrayList<Integer> startIdxList=new ArrayList<Integer>();
              ArrayList<Integer> endIdxList=new ArrayList<Integer>();
              ArrayList<String> headerList=new ArrayList<String>();
              while (inReportList.hasNextLine()) {
                 String[] splitString=inReportList.nextLine().split("[-,]");
                 startIdxList.add(Integer.valueOf(splitString[0]));
                 endIdxList.add(Integer.valueOf(splitString[1]));
                 headerList.add(splitString[2]);
              }
              inReportList.close();

              // output headers to output file
              for (int i=0; i<headerList.size(); i++) {
                 writer.write(headerList.get(i)+"\t");
              }

              // sends report request to c file
              MessageJNI.writeReportRequest(reportIdx, reportCount, searchString);
              String msg;
              ArrayList<String> itemList=new ArrayList<String>();
              // reads matching strings back from c file until empty string
              while (true) {
                 msg=MessageJNI.readReportRecord(reportIdx);
                 // check for empty string
                 if (msg.length()==0) {
                    break;
                 }
                 writer.write("\n");
                 // output to output file with correct formatting
                 for (int i=0; i<startIdxList.size(); i++) {
                    int startHere=startIdxList.get(i)-1;
                    int endHere=endIdxList.get(i);
                    itemList.add(msg.substring(startHere, endHere)+"\t");
                    writer.write(msg.substring(startHere, endHere));
                    writer.write("\t");
                 }
              }
              writer.close();

           } catch(Exception e) {
              e.printStackTrace();
           }
        }

	public ReportingSystem(int idx, int count, File in) {
	  DebugLog.log("Starting Reporting System");
          // variables that need to be accessed in the thread
          this.reportIdx=idx;
          this.reportCount=count;
          this.inputFile=in;
	}

	public int loadReportJobs() {
		int reportCounter = 0;
		try {

			   File file = new File ("report_list.txt");

			   Scanner reportList = new Scanner(file);
                           // set reportCounter to first line of report_list.txt
                           reportCounter=Integer.valueOf(reportList.nextLine());

 		     //load specs and create threads for each report
				 DebugLog.log("Load specs and create threads for each report\nStart thread to request, process and print reports");
                           int idx=1;
                           while (reportList.hasNextLine()) {
                              String line = reportList.nextLine();

                              File in = new File (line);

                              // start thread for each report in report_list.txt
                              ReportingSystem thread=new ReportingSystem(idx, reportCounter, in);
                              thread.start();

                              idx+=1;
                          }

			   reportList.close();
		} catch (FileNotFoundException ex) {
			  System.out.println("FileNotFoundException triggered:"+ex.getMessage());
		}
		return reportCounter;

	}

	public static void main(String[] args) throws FileNotFoundException{


                   try {
                      File temp=File.createTempFile("temp", ".txt");
                      temp.deleteOnExit();
                      ReportingSystem reportSystem= new ReportingSystem(0, 0, temp);
                      int reportCount = reportSystem.loadReportJobs();
                   } catch(Exception e) {
                      e.printStackTrace();
                   }


	}

}
