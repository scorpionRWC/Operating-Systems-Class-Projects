#include <iostream> 
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <semaphore.h>
#include <pthread.h>

using namespace std; 

struct Movie 
{
    string name;
    int seatsLeft;
    sem_t status; // needed to avoid both agents from selling the last seat of the same movie
}*movie;

int movieCount; // number of movies at the theatre
int movieChosenBy[50]; // which movie [i] wants to see
bool soldOut[50]; // whether or not the movie the customer wants to see is sold out
bool endOfSimulation = false;
int snackOrNot[50]; // whether or not to go to the concession stand 
int whichSnack[50]; // if going to the concession stand, get which snacks (as int for rand() function)
string snackChosen[50]; // snacks chosen as string for output

// Semaphores 
sem_t enterBoxOfficeLine; // enqueuing for buying ticket
sem_t customer[50]; // synchronizing customer thread [i]
sem_t boxOfficeAgentWithCustomer[50]; // synchronizing box office agent thread with customer thread [i]
sem_t enterTicketTakerLine; // enqueuing for ticket taker 
sem_t enterConcessionStandLine;// enqueuing for concession stand 
sem_t exitBoxOfficeLine;// dequeueing of box office line
sem_t exitTicketTakerLine; // dequeuing of ticket taker line
sem_t exitConcessionStandLine; // dequeuing of concession stand line
sem_t customerReadyForTicketTaker;// customer entered ticket taker line 
sem_t customerReadyForConcessionStand;// customer entered concession stand line

class QueueLine // for box office, ticket taker and concession stand line
{
private:
    int queue[50];
    int front, back;
public:
    QueueLine() // constructor to intialize the private data of the class
    {
        front = 0;
        back = -1;
    }
    void enqueue(int c)// customer enters line
    {
        back++;
        queue[back] = c;
    }
    int dequeue() // customer exits line
    {
        int c;
        c = queue[front];
        front++;
        return c;
    }
}boxOfficeQueue, ticketTakerQueue, concessionStandQueue;

void initializeMovies(char* filename) // reads file to get number of movies, movie titles, and each movie's capacity
{
    string line;
    ifstream file(filename);
    while (getline(file, line))
        movieCount++;
    file.close();
    int i = 0;
    char * token;
    movie = new Movie[movieCount];
    file.clear();
    file.seekg(0, ios::beg);
    while (getline(file, line))
    {
        token = strtok(&line[0], "\t");
        movie[i].name = token;
        token = strtok(NULL, "\t");
        movie[i].seatsLeft = atoi(token);
        sem_init(&movie[i].status, 0, 1);
        i++;
    }
}

void *startCustomer(void *id) // everything the customer does in order
{
    int customerID = *(int *)id;
    sem_wait(&enterBoxOfficeLine); // critical section to enter queue to buy ticket
    boxOfficeQueue.enqueue(customerID); // entering queue
    sem_post(&enterBoxOfficeLine); // safely entered queue
    sem_wait(&customer[customerID]); // wait for signal from box office agent
    movieChosenBy[customerID] = rand() % (movieCount); // randomly choose a movie
    printf(" Customer %d buying ticket to %s\n", customerID, movie[movieChosenBy[customerID]].name);
    sem_post(&boxOfficeAgentWithCustomer[customerID]); // let agent know which customer is being helped
    sem_wait(&customer[customerID]); // wait for agent to reply
    // Check if the seat is available
    if (soldOut[customerID])
    {
        printf(" Customer %d exits due to no seats available for %s\n", customerID, movie[movieChosenBy[customerID]].name);
    }
    else
    {
        sem_wait(&enterTicketTakerLine); // Critical section to enter ticket taker line 
        ticketTakerQueue.enqueue(customerID); // enter line
        printf(" Customer %d standing in line to see Ticket Taker\n", customerID);
        sem_post(&enterTicketTakerLine); // safely entered line
        sem_post(&customerReadyForTicketTaker); // Signal that customer has entered the queue
        sem_wait(&customer[customerID]);

        snackOrNot[customerID] = rand() % 2; // Decide if the customer wants to enter the concession stand
        if (snackOrNot[customerID] == 0)
        {
            whichSnack[customerID] = rand() % 3; // Generate the concession stand choice
            if (whichSnack[customerID] == 0)
                snackChosen[customerID] = "Popcorn";
            else if (whichSnack[customerID] == 1)
                snackChosen[customerID] = "Soda";
            else if (whichSnack[customerID] == 2)
                snackChosen[customerID] = "Popcorn & Soda";
            sem_wait(&enterConcessionStandLine); // Critical section to enter queue for the concesion stand
            concessionStandQueue.enqueue(customerID);
            printf(" Customer %d standing in line to buy %s\n", snackChosen[customerID]);
            sem_post(&enterConcessionStandLine);
            sem_post(&customerReadyForConcessionStand); // Indicate that customer has entered the queue for concession stand
            sem_wait(&customer[customerID]);
            printf(" Customer %d recieves %s\n", customerID, snackChosen[customerID]);
        }
        printf(" Customer %d enters theatre to see %s\n", customerID, movie[movieChosenBy[customerID]].name);
    }
}

void *boxOfficeAgent(void *boaID)// everything a box office agent does in order
{
    int boxOfficeCustomerID, movieNumber;
    int boxOfficeAgentID = *(int *)boaID;
    printf(" Box Office Agent %d created\n", boxOfficeAgentID);
    while (!endOfSimulation)
    {
        sem_wait(&exitBoxOfficeLine); // critical section to dequeue 
        boxOfficeCustomerID = boxOfficeQueue.dequeue(); // dequeueing
        sem_post(&exitBoxOfficeLine); // safely dequeued
        sem_post(&customer[boxOfficeCustomerID]); // signal ready to customer [i]
        printf(" Box Office Agent %d serving Customer %d\n", boxOfficeAgentID, boxOfficeCustomerID);
        sem_wait(&boxOfficeAgentWithCustomer[boxOfficeCustomerID]); // wait for movie name from customer [i]
        movieNumber = movieChosenBy[boxOfficeCustomerID];
        sem_wait(&movie[movieNumber].status); // make sure other agent's customer is not buying the last seat 
        if (movie[movieNumber].seatsLeft > 0)// Check if seats are available
        { 
            printf(" Box Office Agent %d sold ticket for %s to Customer %d\n", boxOfficeAgentID, movie[movieNumber].name, boxOfficeCustomerID);
            movie[movieNumber].seatsLeft--;
            soldOut[boxOfficeCustomerID] = false;
        }
        else
            soldOut[boxOfficeCustomerID] = true;
        sem_post(&movie[movieNumber].status); // talk to other agent about current movie's status 
        sleep(1.5);
        sem_post(&customer[boxOfficeCustomerID]); // tell customer that task is finished
    }
}

void *ticketAgent(void *a) // everything a ticket taker does in order
{
    int ticketAgentCustomerID;
    cout << " Ticket Taker created\n";

    while (!endOfSimulation)
    {
        sem_wait(&customerReadyForTicketTaker); // wait for customer to enter the queue
        sem_wait(&exitTicketTakerLine); // wait for previous customer to leave
        ticketAgentCustomerID = ticketTakerQueue.dequeue(); // dequeue customer from the queue
        sem_post(&exitTicketTakerLine); // previous customer safely left
        sleep(0.25);
        printf(" Ticket taken from Customer %d\n", ticketAgentCustomerID);
        sem_post(&customer[ticketAgentCustomerID]); // let customer know the ticket was taken
    }
}

void *concessionStand(void *c) // everything happening at the concession stand in order
{
    int concessionStandCustomerID;
    cout << " Concession Stand created\n";

    while (!endOfSimulation)
    {
        sem_wait(&customerReadyForConcessionStand); // wait for customer to enter the queue
        sem_wait(&exitConcessionStandLine); // wait for previous customer to leave
        concessionStandCustomerID = concessionStandQueue.dequeue(); // dequeue customer from the queue
        sem_post(&exitConcessionStandLine); // let next customer know that the stand is ready
        printf(" Order for %s taken from Customer %d\n", snackChosen[concessionStandCustomerID], concessionStandCustomerID);
        sleep(3);
        sem_post(&customer[concessionStandCustomerID]); // let current customer know that order was filled
    }
}

int main(int argc, char *argv[]) // main function
{
    initializeMovies(argv[1]);
   
    // Initializing Semaphores
    sem_init(&enterBoxOfficeLine, 0, 1);
    sem_init(&enterTicketTakerLine, 0, 1);
    sem_init(&enterConcessionStandLine, 0, 1);
    sem_init(&exitBoxOfficeLine, 0, 1);
    sem_init(&exitTicketTakerLine, 0, 1);
    sem_init(&exitConcessionStandLine, 0, 1);
    sem_init(&customerReadyForTicketTaker, 0, 0);
    sem_init(&customerReadyForConcessionStand, 0, 0);
    
    for (int i = 0; i < 50; i++)
        sem_init(&customer[i], 0, 0);
    for (int i = 0; i < 50; i++)
        sem_init(&boxOfficeAgentWithCustomer[i], 0, 0);
    
    // Initializing Threads
    pthread_t customerThread[50]; // 50 customer threads
    pthread_t boxOfficeAgentThread[2]; // 2 box office agent threads
    pthread_t ticketTakerThread; // 1 ticket taker thread
    pthread_t concessionStandThread; // 1 concession stand thread
    
    for (int i = 0; i < 50; i++)
    {
        int *num = new int;
        *num = i; // Creating 50 customer threads
        pthread_create(&customerThread[i], NULL, startCustomer, (void *)num);
    }
    
    for (int i = 0; i < 2; i++)
    {
        int *num = new int;
        *num = i; // creating 2 box office agent threads
        pthread_create(&boxOfficeAgentThread[i], NULL, boxOfficeAgent, (void *)num);
    }

    // creating ticket taker thread
    pthread_create(&ticketTakerThread, NULL, ticketAgent, NULL);
    
    // creating the concession stand thread
    pthread_create(&concessionStandThread, NULL, concessionStand, NULL);

    for (int i = 0; i < 50; i++)
    { // Rejoining customer threads
        pthread_join(customerThread[i], NULL);
        printf("\n Joined customer %d", i);
    }

    cout << endl;
    endOfSimulation = true;
    // End of Program
    return 0;
}