#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "report_record_formats.h"
#include "queue_ids.h"

pthread_mutex_t lock;
pthread_cond_t outputStatus;

// global variables that will need to be accessed in status report thread
int totalRecords=0;
int reportCount=0;
int eof=0;
int *recPerRept;

// signal handler signals status report thread
void signal_handler()
{
   pthread_cond_signal(&outputStatus);
}

// print status report
void* statusReport(void* args) {
   while (1) {
      pthread_cond_wait(&outputStatus, &lock);
      fprintf(stdout, "***Report***\n");
      fprintf(stdout, "%d records read for %d reports\n", totalRecords, reportCount);
      for (int i=0; i<reportCount; i++) {
         fprintf(stdout, "Records sent for report index %d: %d\n", i+1, recPerRept[i]);
      }
      if (eof==1) break;
   }
}

int main(int argc, char**argv)
{
   // initialize condition variable
   pthread_cond_init(&outputStatus, NULL);

   // create status report thread
   pthread_t status;
   pthread_create(&status, NULL, statusReport, (void*) 0);

   int msqid;
   int msgflg = IPC_CREAT | 0666;
   key_t key;
   report_request_buf rbuf;
   report_record_buf sbuf;
   size_t buf_length;

   signal(SIGINT,  signal_handler);

   // make sure queue_ids.h is set up correctly
   key = ftok(FILE_IN_HOME_DIR,QUEUE_NUMBER);
   if (key == 0xffffffff) {
      fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
      return 1;
   }

   // prep to receive messages
   if ((msqid = msgget(key, msgflg)) < 0) {
      int errnum = errno;
      fprintf(stderr, "Value of errno: %d\n", errno);
      perror("(msgget)");
      fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
   }
   else
      fprintf(stderr, "msgget: msgget succeeded: msgqid = %d\n", msqid);

   key = ftok(FILE_IN_HOME_DIR,QUEUE_NUMBER);
   if (key == 0xffffffff) {
      fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
      return 1;
   }

   // receive first message from java side
   int ret;
   do {
      ret = msgrcv(msqid, &rbuf, sizeof(rbuf), 1, 0);//receive type 1 message
      fprintf(stderr, "rbuf: %s\n", rbuf.search_string);

      int errnum = errno;
      if (ret < 0 && errno !=EINTR){
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("Error printed by perror");
        fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
      }
    } while ((ret < 0 ) && (errno == 4));
   report_request_buf rcv_arr[rbuf.report_count];
   rcv_arr[0]=rbuf;

   fprintf(stderr,"process-msgrcv-request: msg type-%ld, Record %d of %d: %s ret/bytes rcv'd=%d\n", rbuf.mtype, rbuf.report_idx,rbuf.report_count,rbuf.search_string, ret);
   reportCount=rbuf.report_count;
   recPerRept=malloc(sizeof(int)*reportCount);

   // receive second-nth messages from java side
   for (int i=1; i<rbuf.report_count; i++) {
      int ret;
      do {
         ret = msgrcv(msqid, &rbuf, sizeof(rbuf), 1, 0);//receive type 1 message
         fprintf(stderr, "rbuf: %s\n", rbuf.search_string);

         int errnum = errno;
         if (ret < 0 && errno !=EINTR){
           fprintf(stderr, "Value of errno: %d\n", errno);
           perror("Error printed by perror");
           fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
         }
       } while ((ret < 0 ) && (errno == 4));
      rcv_arr[i]=rbuf;

      fprintf(stderr,"process-msgrcv-request: msg type-%ld, Record %d of %d: %s ret/bytes rcv'd=%d\n", rbuf.mtype, rbuf.report_idx,rbuf.report_count,rbuf.search_string, ret);
   }


   char input[RECORD_MAX_LENGTH];
   int recCounter=0;
   for (int i=0; i<reportCount; i++) {
      recPerRept[i]=0;
   }

   // loop through every line of stdin
   while (fgets(input, RECORD_MAX_LENGTH, stdin)!=NULL) {
      // sleep for 5 seconds after reading in 10 records
      if (recCounter==10) {
         //sleep(5);
         time_t start=time(NULL);
         time_t current=time(NULL);
         // does the same thing as sleep(5) but doesn't wake up
         // after SIGINT handling
         while (current-start<5) {
            current=time(NULL);
         }
         recCounter=0;
      }
      recCounter++;
      totalRecords++;
      // loop through all reports
      for (int i=0; i<rbuf.report_count; i++) {
         key = ftok(FILE_IN_HOME_DIR,rcv_arr[i].report_idx);
         if (key == 0xffffffff) {
             fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
             return 1;
         }

         if ((msqid = msgget(key, msgflg)) < 0) {
             int errnum = errno;
             fprintf(stderr, "Value of errno: %d\n", errno);
             perror("(msgget)");
             fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
         }
         else

          fprintf(stderr, "msgget: msgget succeeded: msgqid = %d\n", msqid);

         // if the search string is found in stdin, send message to java side
         if (strstr(input, rcv_arr[i].search_string)) {
            recPerRept[rcv_arr[i].report_idx-1]++;
            // We'll send message type 2
            sbuf.mtype = 2;
            strcpy(sbuf.record, input);
            buf_length = strlen(sbuf.record) + sizeof(int)+1;//struct size without
            // Send a message.
            if((msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT)) < 0) {
                int errnum = errno;
                fprintf(stderr,"%d, %ld, %s, %d\n", msqid, sbuf.mtype, sbuf.record, (int)buf_length);
                perror("(msgsnd)");
                fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
                exit(1);
            }
            else
                fprintf(stderr,"msgsnd-report_record: record\"%s\" Sent (%d bytes)\n", sbuf.record,(int)buf_length);
         }
         fprintf(stderr,"records for report with index %d: %d", rcv_arr[i].report_idx, recPerRept[rcv_arr[i].report_idx-1]);
      }
   }

   // send zero length string to java side to signify termination
   for (int i=1; i<rbuf.report_count+1; i++) {
      key = ftok(FILE_IN_HOME_DIR,i);
      if (key == 0xffffffff) {
          fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
          return 1;
      }

      if ((msqid = msgget(key, msgflg)) < 0) {
          int errnum = errno;
          fprintf(stderr, "Value of errno: %d\n", errno);
          perror("(msgget)");
          fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
      }
      else

      fprintf(stderr, "msgget: msgget succeeded: msgqid = %d\n", msqid);
      // We'll send message type 2
      sbuf.mtype = 2;
      sbuf.record[0]=0;
      buf_length = strlen(sbuf.record) + sizeof(int)+1;//struct size without
      // Send a message.
      if((msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT)) < 0) {
          int errnum = errno;
          fprintf(stderr,"%d, %ld, %s, %d\n", msqid, sbuf.mtype, sbuf.record, (int)buf_length);
          perror("(msgsnd)");
          fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
          exit(1);
      }
      else
          fprintf(stderr,"msgsnd-report_record: record\"%s\" Sent (%d bytes)\n", sbuf.record,(int)buf_length);
   }

   // set eof flag to 1 to break status report thread out of while loop
   eof=1;
   signal_handler();
   pthread_join(status, NULL);
   free(recPerRept);

   exit(0);
}
