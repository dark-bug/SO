
 void load_stats(){
 	FILE *f = fopen(STATS_FILE,"r");

 	if(f){
 		printf("\nPath is valid.\n");


 		char line[SIZE_BUF*2],*temp;
 		int i=0;
 		stats aux;
 		while(fgets(line,sizeof(line),f)!=NULL){
 			temp = strtok(line,COMMA);
 			for(i=0;i<5;i++){
 				
 				switch(i){
 					case 0:
 					memcpy(aux.request_type,temp,strlen(temp));
 					break;
 					case 1:
 					memcpy(aux.filename,temp,strlen(temp));
 					break;
 					case 2:
 					memcpy(aux.thread_number,temp,strlen(temp));
 					break;
 					case 3:
 					memcpy(aux.reception_time,temp,strlen(temp));
 					break;
 					case 4:
 					memcpy(aux.conclusion_time,temp,strlen(temp));
 					break;
 				}
 				temp = strtok(NULL,COMMA);
 			}
 		}
 		printf("Loaded statistics from file.\n\n");
 		if(msgsnd(m_queue,&aux,sizeof(aux),0)!=-1)
 			printf("Sent statistics to message queue.\n");
 		else{
 			printf("Error sending statistics to message queue.\n");
 			system("pause");
 			exit(0);
 		}

 		if(msgrcv(m_queue,&aux,sizeof(aux),0,0)!=-1)
 			printf("Received statistics in message queue.\n\n");
 		else{
 			printf("Error receiving statistics to message queue.\n");
 			system("pause");
 			exit(0);
 		}
 	}
 	else{
 		printf("\nPath is not valid\n");
 		system("pause");
 		exit(0);
 	}

 	fclose(f);
 }
