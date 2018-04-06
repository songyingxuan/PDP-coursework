/*
 * Example code to run and test the process pool. To compile use something like mpicc -o test test.c pool.c
 */

#include <stdio.h>
#include "mpi.h"
#include "pool.h"
#include<time.h>
#include <unistd.h>
#include "squirrel-functions.h"

static void workerCode();

int main(int argc, char* argv[]) {
	// Call MPI initialize first
	MPI_Init(&argc, &argv);
	/*
	 * Initialise the process pool.
	 * The return code is = 1 for worker to do some work, 0 for do nothing and stop and 2 for this is the master so call master poll
	 * For workers this subroutine will block until the master has woken it up to do some work
	 */
	int statusCode = processPoolInit();
	// printf("statusCode = %d\n", statusCode);
	if (statusCode == 1) {
		// A worker so do the worker tasks
		workerCode();
	} else if (statusCode == 2) {
		/*
		 * This is the master, each call to master poll will block until a message is received and then will handle it and return
		 * 1 to continue polling and running the pool and 0 to quit.
		 * Basically it just starts 51 workers and then registers when each one has completed. When they have all completed it
		 * shuts the entire pool down
		 */
		int i,j, activeWorkers=0;
		//define Ran_inf_squ array to store four random rank number which has infected squirrels
		int Ran_inf_squ[4];
		//this array used to strore initial status of 34 initial squirrels, 1 represents infected, 0 means healthy
		int infec_status[34]={0};
		
		int tag,squirrel_sum =34;
		
			
		MPI_Request initialWorkerRequests[50];
		MPI_Status returnCode,cell_return;

		//this part is to produce random rank number from 17 to 50 which has infected squirrels
		srand(time(NULL));
		for(i=0;i<4;i++){
			Ran_inf_squ[i] = rand()%34+17;
		}
		//if the two random number is the same, produce another random number to ensure that 4 infected squirrels are in different processes
		for (i=0;i<4;i++)
		{
			for(j=i+1;j<4;j++)
			{
				if(Ran_inf_squ[i]==Ran_inf_squ[j]) Ran_inf_squ[i] = rand()%35+17;
			}	
		}

		//start 16 cell actors from rank number 1 to 16 
		for(i=1;i<17;i++){
			int workerPid = startWorkerProcess();
			activeWorkers++;
		}

		
		//start squirrels from 17 to 50 and send their status 
		for (i=17;i<51;i++) {
			int workerPid = startWorkerProcess();
			//set the four random rank with infected squirrels
			for(j=0;j<4;j++){
				if(Ran_inf_squ[j]==i)  infec_status[i-17] = 1;
			}
			//printf("the workPid %d, infection status is %d \n",workerPid,infec_status[i-18]);
			MPI_Isend (&infec_status[i-17], 1, MPI_INT, i, 0, MPI_COMM_WORLD, &initialWorkerRequests[i]);
			//MPI_Irecv(NULL, 0, MPI_INT, workerPid, 0, MPI_COMM_WORLD, &initialWorkerRequests[i]);
			activeWorkers++;
		}

		
		int masterStatus = masterPoll();
		//printf("masterStatus is %d\n",masterStatus);		

		
		while (masterStatus) {
			masterStatus=masterPoll();
		}
		printf("squirrel sum is %d, and it finished\n",squirrel_sum);

		// Finalizes the process pool, call this before closing down MPI
		processPoolFinalise();
		// Finalize MPI, ensure you have closed the process pool first
		MPI_Finalize();
		return 0;
		}
	}

	static void workerCode() {
		int workerStatus = 1;
		int myRank, parentId;
		MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

		long state = -1-myRank;	
		initialiseRNG(&state);

		if (myRank>16){
			int should_stop = shouldWorkerStop();
			 int step=0,i,num_step;
				//infec_status represent squirrels infected or not,infec_status = 1, squirrels are infected,
				//child_infec_status stores status of new born squirrels,they reveived from parents
                                int  infec_status,child_infec_status = 0;
                                float x,y,x_new,y_new;
				// rank nummber of current cells and next cells
                                int next_cell,current_cell;
				//record the information of population and infection of cells visited in last 50 steps 
                                int population_influx[50]={0},infection_influx[50]={0};
                                int pop_sum=0,infec_sum=0;
                                float avg_pop=0,avg_inf_level=0;
                                //int *buffer = (int *)malloc(10000);
                                int *de_buffer;
                                int size;
				//record steps moved after infection
                                int catch_step;
				//messages received from cells, cell_info[0] stores population influx, cell_info[1] saves infection level of the cell
                                int cell_info[2]={0};
                  		//rank number of new born squirrels
		                int childPid;
				//position of parent and children, parent_pos[0] =x, parent_pos[1] = y
                                float parent_pos[2],child_pos_start[2];
			

                                MPI_Request request_new,request_new1,request_to_child,request;
                                MPI_Status status,status_new,status_new1,childRequest;

				//initialise the populaition and infection influx array
				for( i=0;i<50;i++){
                                 population_influx[i] = 0;
                                 infection_influx[i] = 0;
                                        }
				
				int parentId = getCommandData();

				//if actors are created by master, set the initial position to be (0,0)
                                if(parentId == 0)
                                {
                                        x=0;
                                        y=0;
                                }
				
				//if squirrels are born from existing squirrels, use MPI_Recv to know the start position 
                                else{
                                        MPI_Recv(child_pos_start,2,MPI_FLOAT,parentId,0,MPI_COMM_WORLD,&childRequest);
                                        x = child_pos_start[0];
                                        y = child_pos_start[1];
                                }

				//before squirrels move, MPI_Recv to receive initial status from parents 
                                MPI_Recv(&infec_status,1,MPI_INT,parentId,0,MPI_COMM_WORLD,&status);
				//MPI_Recv(&infec_status,1,MPI_INT,parentId,0,MPI_COMM_WORLD,&status);			        
				printf(" rank: %d infect status: %d,position is (%f,%f), parentId is %d\n",myRank,infec_status,x,y,parentId);
				
			while(should_stop  == 0 && workerStatus ==1 ){
				
					usleep((int)(10000));
					should_stop = shouldWorkerStop();
				if(should_stop ==0){
					squirrelStep(x,y,&x_new,&y_new,&state);
					 if(infec_status==1){
                                                catch_step++;
                                        }
                                                                                                                                                                                                            
					//printf("squirrels hop to (%f,%f) \n",x_new,y_new);

					next_cell = getCellFromPosition(x_new,y_new)+1;
				
					MPI_Isend(&infec_status,1,MPI_INT,next_cell,0,MPI_COMM_WORLD,&request);
					//MPI_Send(&infec_status,1,MPI_INT,next_cell,0,MPI_COMM_WORLD);
					MPI_Recv(cell_info, 2, MPI_INT, next_cell, 0, MPI_COMM_WORLD,&status);
					//printf(" Rank %d squirrel hop to %d\n", myRank, next_cell);
					MPI_Wait(&request,&status_new);
					//printf("bfbhjvfhjsv\n");
					
					population_influx[step%50] = cell_info[0];
                                        infection_influx[step%50] = cell_info[1];
				      //printf("the pop receive is %d,the infect receives is %d\n",cell_info[0],cell_info[1]);
				//	printf("Rank %d, infection_influx[%d] is %d\n",myRank,step%50,infection_influx[step%50]);	
					
					//calculate the average infection level and population influx of last 50 steps
                                        num_step = 50;
                                        if(step<50){ 
					num_step = step;
						}
                                        for(i=0;i<num_step;i++){
                                        	pop_sum += population_influx[i];
                                        	infec_sum += infection_influx[i];
                                        	avg_inf_level = (float)infec_sum/(float)num_step;
                                        	avg_pop = (float)pop_sum/(float)num_step;
				//printf("Rank %d,pop_sum is %d, infec_sum is %d, avg_inf_level is %d, the infection_influx[%d] is %d\n",myRank,pop_sum,infec_sum,avg_inf_level,i,infection_influx[i]);
                                        	}
					//printf("pop_sum is %d, infec_sum is %d, avg_inf_level is %d\n",pop_sum,infec_sum,avg_inf_level);
					
	
		
					//check squirrels catch disease or not with average infection level
					 if(infec_status != 1 && shouldWorkerStop()==0){
                                                if(willCatchDisease(avg_inf_level,&state)){
							infec_status = 1;
							}
    						}
					
					printf("rank %d will_catch_disease:%d\n",myRank,willCatchDisease(avg_inf_level,&state));			
				
					//squirrels may reproduce every 50 steps
					if(step%50==0){
						//printf("avg_pop is %d,wiil_giveBirth = %d\n",avg_pop,willGiveBirth(avg_pop,&state));
						if (willGiveBirth(avg_pop,&state)!=0){
							//if squirrels breed, set current position to parent_pos for sending 
							parent_pos[0] = x;
							parent_pos[1] = y;
							//childPid record the rank number of new squirrels returned from process pool
							childPid = startWorkerProcess();
							current_cell = getCellFromPosition(x, y);
							printf("rank %d  breeds child %d at (%f,%f)\n",myRank,childPid,parent_pos[0],parent_pos[1]);
							//parent squirrels send their position and infec_status to child in order 
							//parent send position first and then infex_status in order to match the MPI receive order before squirrels move
							MPI_Isend(parent_pos,2,MPI_FLOAT,childPid,0,MPI_COMM_WORLD,&request_new);
							printf("the status of child is %d",child_infec_status);
							MPI_Isend(&child_infec_status,1,MPI_INT,childPid,0,MPI_COMM_WORLD,&request_new1);
							//send messages to current cells to tell that new squirrels born with infec_status, which is 0 at begin 
							MPI_Isend(&child_infec_status,1,MPI_INT,current_cell,0,MPI_COMM_WORLD,&request_to_child);
							//MPI_Send(NULL,0,MPI_INT,0,1,MPI_COMM_WORLD);  //send message to current cell to tell new squirrel is born with tag=1
						}
					//printf("the infect status is %d\n",infec_status);
					}	
					
					//check the infected squirrels will step for 50 steps
					if(catch_step>=50){
						if(willDie(&state)==1) {
							workerStatus = workerSleep();
						}
					}

					//update the position after one step
					x = x_new;
					y = y_new;
					step++;
				      }
				//send messages to master that this squirrel dies 
				// MPI_Send(NULL, 0, MPI_INT, 0, 0, MPI_COMM_WORLD);
				}
			//MPI_Send(NULL, 0, MPI_INT, 0, 0, MPI_COMM_WORLD);  
			}


		//for cell actors, assign rank 1-16 are cell actors
		if(myRank< 17 && myRank > 0){
			
			int population[3]={0},infection[2]={0};
			int populationInflux=0,infectionLevel=0,population_total=0;
			int rev,squ_population=0,infec_squ=0;
			int i,j,finish=9999;
			int send_inf[2]={0};
			int send_to;
			int flag=0;
			MPI_Request clock_request,request_cell;
			MPI_Status status1;
			double start,end;		

			// parentId = getCommandData();
			// MPI_Send(NULL, 0, MPI_INT, parentId, 0, MPI_COMM_WORLD);    			//send message back to master to indicate the cells are active	

			printf("******cells:Rank %d ****************\n",myRank);
			for(i=0;i<24;i++){
				start = MPI_Wtime();
				while((MPI_Wtime()-start)<2){
					MPI_Iprobe(MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&flag,&status1);
					if(flag == 0) {
						continue;
					} else {
						MPI_Recv(&rev,1,MPI_INT,MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status1);
						send_to = status1.MPI_SOURCE;
						//printf("the status %d receives from %d\n",rev,send_to);
						//printf("cell: Rank %d receives message from %d\n",myRank,send_to);
					
					//rev is the infec_status of squirrels, if = 1, the squirrel is infected, or =0, squirrel is healthy
						if(rev==1){						
							squ_population++;
							infec_squ++;	
						}
						else{ 
							squ_population++;
						}
		   			 
						
						//put current pupulation and infectin level to arrays corresponds to last 2 or 3 months 		
						population[i%3] = squ_population;
                                                infection[i%2] = infec_squ;
                      
                                                populationInflux = population[0]+population[1]+population[2];
                                                infectionLevel = infection[1]+ infection[0];
						
						//if no squirrels comes to this cell, the infection level will be 0
						//because virus cannot live independently for 2 months in the environment
						
                                                if(population[(i-1)%50] == population[(i-2)%50]) infectionLevel = 0;						

						//printf("the populationInflux is %d,infectionLevel is %d\n",populationInflux,infectionLevel);
						send_inf[0] = populationInflux;
                                                send_inf[1] = infectionLevel;
                                                MPI_Isend(send_inf, 2, MPI_INT,send_to, 0, MPI_COMM_WORLD,&request_cell);
					}


				}
					 squ_population =0;
                                          infec_squ = 0;
				
				printf("Rank %d, Month %d,The population influx: %d, infection level: %d  \n",myRank,i,populationInflux,infectionLevel);
				printf("Month %d,The infection level: %d \n ",i,infectionLevel);	
			}
			shutdownPool();

		}
	}
	

