#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <jansson.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define QUEUESIZE 500
#define REQUEST_SIZE 4
#define MAXFLOAT 0x1.fffffep+127f

//connection context
struct lws_context *context;
//connection WSI
static struct lws *client_wsi = NULL;

//mutex for the counter thread
pthread_mutex_t cntr_mut;

//Variables relative to current time estimation
time_t now;
struct tm *t;
char time_str[40];

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

//Files storing the trades & candlesticks
FILE *file[REQUEST_SIZE*2];

//Boolean values for interruption 
static bool exit_requested = false;
static bool interrupted = false;
static bool consumer_can_exit = false;

//Count of pings
static int times_pinged = 0;

//Symbols requesting for data
const char* symbols[REQUEST_SIZE] = {"BINANCE:ETHUSDT","AMZN","MSFT","BTC"}; //AAPL AMZN BINANCE:ETHUSDT COINBASE:ETH-USD



//Global variables related to candlestick generation
double minute_prices[REQUEST_SIZE] = {0};
static int number_of_trades[REQUEST_SIZE] = {0};
float price[REQUEST_SIZE] = {0};
float maxp[REQUEST_SIZE] = {0};
long long volume_sum[REQUEST_SIZE] = {0};
float minp[REQUEST_SIZE] = {MAXFLOAT};
bool flag_init[REQUEST_SIZE] = {false};
float start_price[REQUEST_SIZE] = {0};




//3 types of threads generated by program
void *producer();
void *consumer();
void *counter();

//Functions related to client connection
static struct lws *connect_to_server(struct lws_context *context);
static struct lws_context *create_context(void);

//info after parsing
typedef struct{
  long long int t;
  long long int v;
  float p;
  const char* s;
} input_info;

//Queue for producer-consumer
typedef struct{
	input_info buf[QUEUESIZE];
	long head, tail;
	int full,empty;
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
} queue;

//Queue for saving 15-minute data
typedef struct{
  float buff[REQUEST_SIZE][15];
  long tail[REQUEST_SIZE];
} queue2;

void save_string(input_info data);


//producer-consumer queue
queue *fifo;
//container of 15-minute volumes
queue2 *volumes;
//container of 15-minute mean prices
queue2 *mean_prices;

//Methods related to queue functionality
queue *queueInit(void);
queue2 *queue2Init(void);
void queueDelete(queue *q);
void queue2Delete(queue2 *q);
void queueAdd(queue *q, input_info p);
void queue2Add(queue2 *q, int index, long long in);
void queueDel(queue *q, input_info* out);

//Handler of sigint signal
static void sigint_handler(int sig);

void parse_message(char* in);

//Function is called every time the server sends a message
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:   //Connection established
            printf("Connection established\n");
            // Send subscription message
            {
              for(int i = 0; i < 4; i++)
              {
                char msg[64];
                sprintf(msg,"{\"type\":\"subscribe\",\"symbol\":\"%s\"}",symbols[i]);
                size_t len_msg = strlen(msg);
                unsigned char buf1[LWS_SEND_BUFFER_PRE_PADDING + len_msg + LWS_SEND_BUFFER_POST_PADDING];
                memcpy(&buf1[LWS_SEND_BUFFER_PRE_PADDING], msg, len_msg);
                int n = lws_write(wsi, &buf1[LWS_SEND_BUFFER_PRE_PADDING], len_msg, LWS_WRITE_TEXT);
                if (n < 0) 
                    printf("Failed to send subscription message for %s\n",symbols[i]);
              }
            }
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE: //Recieved a message response
            printf("Received message: %s\n \t\t\t\t\t\t\t\t @ time %ld\n", (char *)in,time(NULL));
            if(len > 0)
              parse_message((char *) in);
            else
              fprintf(stderr,"len less than or equal to 0\n");
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            interrupted = true;
            printf("Connection error\n");
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            interrupted = true;
            printf("Connection closed\n");
            break;
        default:
            break;
    }
    return 0;
}

//protocols for connection
static struct lws_protocols protocols[] = {
  {
    "websocket_protocol",
    ws_callback,
  },
  {NULL, NULL, 0, 0} // Terminator
};

int main() {
  //make all threads
	pthread_t pro,con1,con2,con3,cntr;
  
  //get current time
  now = time(NULL);
  t = localtime(&now);
  strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S.json", t);
  
  //open all files
  for(int i = 0; i < REQUEST_SIZE*2; i++)
  {
    char filename[20];
    if(i < REQUEST_SIZE)
      sprintf(filename,"%s.txt",symbols[i]);
    else
      sprintf(filename,"%s_c.txt",symbols[i-REQUEST_SIZE]);
    file[i] = fopen(filename,"w");
    if(file[i] == NULL)
    {
      printf("Error opening file\n");
      return -1;
    }
  }
  
  //initialize cntr mutex
  pthread_mutex_init(&cntr_mut,NULL);

  //connect sigint to handler
  signal(SIGINT, sigint_handler);

  //initialize queues
	fifo = queueInit();
  volumes = queue2Init();
  mean_prices = queue2Init();

	if(fifo == NULL || volumes == NULL || mean_prices == NULL){
		fprintf(stderr, "main: Queue Init failed.\n");
		return -1;
	}
  
  //Create all threads
  pthread_create(&pro, NULL, producer, NULL);
	pthread_create(&con1, NULL, consumer, NULL);
  //pthread_create(&con2, NULL, consumer, NULL);
  //pthread_create(&con3, NULL, consumer, NULL);
  pthread_create(&cntr, NULL, counter, NULL);
	pthread_join(pro,NULL);
	pthread_join(con1,NULL);
  //pthread_join(con2,NULL);
  //pthread_join(con3,NULL);
  pthread_join(cntr,NULL);

  return 0;
}


static void sigint_handler(int sig){
  exit_requested = true;
  pthread_cond_signal(&cond);
}

void *producer()
{
  context = create_context();
  if (!context) return (NULL);
  while (!exit_requested) 
  {
  if (!client_wsi || interrupted) {
      if (interrupted) {
          printf("Reconnecting...\n");
          sleep(5); // Wait before reconnecting
      }
      client_wsi = connect_to_server(context);
      interrupted = false;
    }
  lws_service(context, 1000);

  if (interrupted){
      lws_context_destroy(context);
      context = create_context();
      if (!context) break;
    }
  } 

  //exit cleanup

  printf("Exiting...\n");
  consumer_can_exit = true;
  pthread_cond_signal(fifo->notEmpty);
  //give time for consumers to consume everything
  usleep(20000);
  printf("after sleep\n");
  lws_context_destroy(context);
  printf("destroyed context\n");
  queue2Delete(volumes);
  printf("deleted volumes\n");
  queue2Delete(mean_prices);
  printf("deleted mean_prices\n");
  for(int i = 0; i < REQUEST_SIZE*2; i++){
    if(file[i]!=NULL)
      if(fclose(file[i]) != 0)
        printf("Couldn't close file\n");
  }
  printf("closed files\n");
  printf("Getting into fifo deletion...\n");
  queueDelete(fifo);  
  printf("deleted fifo\n");
  pthread_mutex_destroy(&cntr_mut);
  printf("deleted cntr mutex\n");
  return (NULL);
}

void *consumer()
{
  input_info msg;
  while(1)
  {
    pthread_mutex_lock(fifo->mut);
    while (fifo->empty && !consumer_can_exit) {
      //printf("consumer: queue EMPTY.\n");
      pthread_cond_wait(fifo->notEmpty,fifo->mut);
    }
    if(consumer_can_exit && fifo->empty){
        printf("\nexiting consumer\n");
        return NULL;
      }

    
    queueDel(fifo, &msg);
    printf("CONSUMER DELETED ITEM FROM QUEUE\n");
    
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);
    pthread_mutex_lock(&cntr_mut);
    save_string(msg);
    pthread_mutex_unlock(&cntr_mut);
  }
  return (NULL);
}
queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}

queue2 *queue2Init(void){
  queue2 *q;

  q = (queue2 *)malloc( sizeof(queue2));
  if(q == NULL) return (NULL);
  for(int index = 0; index < 15; index++){
    for(int j = 0; j < REQUEST_SIZE; j++){
      q->buff[j][index] = 0;  
      q->tail[j] = 0;
    }
    
  }
  return q;
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queue2Delete(queue2 *q){
  free(q);
}

void queueAdd (queue *q, input_info p)
{
  q->buf[q->tail] = p;
  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queue2Add(queue2 *q,int index, long long in){
  q->buff[index][q->tail[index]] = in;
  q->tail[index]++;
  if(q->tail[index] == 15)
    q->tail[index] = 0;
}

void queueDel (queue *q, input_info* out)
{
  *out = q->buf[q->head];
  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}
long long queue2Get(queue2 *q, int index){
  long long sum = 0;
  for(int i = 0; i < 15; i++){
    sum += q->buff[index][i];
  }
  return sum;
}



void save_string(input_info data){
    for(int i = 0; i < REQUEST_SIZE; i++)
    {
        if(strcmp(data.s,symbols[i]) == 0)
        {
          fprintf(file[i],"p:%f\ts:%s\tt:%lld\tv:%lld\n",data.p,data.s,data.t,data.v);
          if(data.p > maxp[i]) maxp[i] = data.p;
          if(minp[i] > data.p) minp[i] = data.p;
          if(!flag_init[i])
          {
            flag_init[i] = true;
            start_price[i] = data.p;
          }
          price[i]=data.p;
          volume_sum[i] += data.v;
          minute_prices[i] += data.p;
          number_of_trades[i]++;
          //printf("i:%d INDEX:%d\n",i,number_of_trades[i]);
        }
    }
}  


void *counter(){
  printf("ENTERED COUNTER");
  //open files and stuff
  struct timespec timeout;
  while(1)
  {
    printf("\n\nINSIDE COUNTER\n\n");
    clock_gettime(CLOCK_REALTIME, &timeout);
    printf("got time\n");
    timeout.tv_sec += 60;
    //pthread_mutex_lock(&lock);
    printf("entering while");
    while(!exit_requested){
      int ret = pthread_cond_timedwait(&cond, &lock, &timeout);
      printf("RET=%d\n",ret);
      if (ret == ETIMEDOUT) {
          printf("Thread woke up after timeout.\n");
          break;
      }
    }
    if (exit_requested) {
      printf("Counter thread exiting!\n");
      //pthread_mutex_unlock(&lock);
      return NULL;
    }
    pthread_mutex_lock(&cntr_mut);

    for(int i = 0; i < REQUEST_SIZE; i++)
    {
      if(maxp[i]!= 0 && MAXFLOAT - minp[i] > 0.1 && number_of_trades[i] != 0)
      {
        double mean_minute_price = minute_prices[i]/number_of_trades[i];
        fprintf(file[REQUEST_SIZE+i],"s=%s\tmax = %f\tmin = %f\tv=%lld\tinit_price=%f\t final_price = %f\n",symbols[i],maxp[i],minp[i],volume_sum[i],start_price[i],price[i]);
        queue2Add(mean_prices,i,mean_minute_price);
        queue2Add(volumes,i,volume_sum[i]);
        fprintf(file[REQUEST_SIZE+i],"volume in 15 mins: %lld, mean price in 15 mins: %f\n",queue2Get(volumes,i),queue2Get(mean_prices,i)/15);
      }else{
        printf("no data for %s this min\n",symbols[i]);
      }
      //reset max and min float values for the next min
      maxp[i] = 0;
      minp[i] = MAXFLOAT;
      flag_init[i] = false;
      start_price[i] = 0;
      volume_sum[i] = 0;
      minute_prices[i] = 0;
      number_of_trades[i] = 0;
    }
    pthread_mutex_unlock(&cntr_mut);
  }
  printf("counter returning\n");
  return (NULL);
}

static struct lws_context *create_context(void){
  struct lws_context_creation_info info;
  memset(&info, 0, sizeof(info));
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.iface = NULL;

  struct lws_context *context = lws_create_context(&info);
  if (!context) {
      fprintf(stderr, "Error creating context\n");
  }
  return context;
}

static struct lws *connect_to_server(struct lws_context *context){
  struct lws_client_connect_info client;
  memset(&client, 0, sizeof(client));
  client.context = context;
  client.address = "ws.finnhub.io";
  client.port = 443;
  client.path = "/?token=cqlndu9r01qo3h6tkppgcqlndu9r01qo3h6tkpq0";
  client.host = client.address;
  client.origin = client.address;
  client.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
  client.protocol = protocols[0].name;

  return lws_client_connect_via_info(&client);


}

void parse_message(char *in)
{
  json_error_t error;
  json_t *whole_msg = json_loads((char *)in, 0, &error);
  if (!whole_msg) {
    fprintf(stderr, "Error getting JSON from server: %s\n", error.text);
  }
  json_t *data_array = json_object_get(whole_msg, "data");
  if (!json_is_array(data_array)) {

      json_t *type = json_object_get(whole_msg,"type");
      if(!json_is_string(type)){
        printf("type is not an array \n");
        fprintf(stderr, "\"data\" is not an array or does not exist.\n");
        json_decref(whole_msg);
        return ;
      } 
      if(strcmp("ping",json_string_value(type)) == 0){
        if(++times_pinged == 3){
          interrupted = 1;
          times_pinged = 0;
        }
      printf("TIMES PINGED = %d\n",times_pinged);
      return ;
      }
  }    
  times_pinged = 0;
  for(int i = 0; i < (int)json_array_size(data_array); i++) {
      // Access the fields in each object
      json_t *data = json_array_get(data_array,i);
      if(!json_is_object(data)) // checking if the extracted element is a JSON object
      {
        fprintf(stderr, "Error: Data %d is not an object\n", i);
        lws_context_destroy(context);
        json_decref(data);//see this line again
        exit(EXIT_FAILURE);
      }//dont forget to json_decref them in the end i guess
      json_t *c_array = json_object_get(data, "c");
      json_t *p_value = json_object_get(data, "p");
      json_t *s_value = json_object_get(data, "s");
      json_t *t_value = json_object_get(data, "t");
      json_t *v_value = json_object_get(data, "v");


      if (!c_array || !p_value || !s_value || !t_value || !v_value) {
        fprintf(stderr, "Error: One or more JSON values are NULL.\n");
        return;  // or handle the error appropriately
      }
      input_info curr_info = {.t = json_integer_value(t_value), .v = json_integer_value(v_value), .p = json_real_value(p_value), .s = json_string_value(s_value)};
      if(!fifo->full){
        pthread_mutex_lock (fifo->mut);
        queueAdd(fifo, curr_info);
        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notEmpty);
        printf("ADDED ITEM TO QUEUE\n");
      }else{
        while (fifo->full) {
          printf ("producer: queue FULL.\n");
          pthread_cond_wait (fifo->notFull, fifo->mut);
        }
        pthread_mutex_lock (fifo->mut);
        queueAdd(fifo, curr_info);
        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notEmpty);
        printf("ADDED ITEM TO QUEUE\n");
      }
  }
  json_decref(whole_msg);
  json_decref(data_array);
  return;
}