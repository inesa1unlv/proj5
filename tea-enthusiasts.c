#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdbool.h>
//ingredient indices
#define GREEN_TEA 0
#define BLACK_TEA 1
#define BERGAMOT 2
#define HIBISCUS 3
#define PUFFED_RICE 4
#define SPICES 5
#define NUM_INGREDIENTS 6
#define NUM_SUPPLIERS 6
#define NUM_ENTHUSIASTS 14
//global variables
int pouches[NUM_INGREDIENTS] = {0};
pthread_mutex_t mutex;
sem_t supplier_sem[NUM_INGREDIENTS];
bool program_running = true;
//ingredient names for printing
const char* ingredient_names[] = {
    "Green Tea Leaf",
    "Black Tea Leaf", 
    "Bergamot Oil",
    "Hibiscus Leaf",
    "Puffed Rice",
    "Spice"
};
//supplier thread data
typedef struct{
    int ingredient_id;
    int pouches_to_add;
} supplier_data_t;

//enthusiast thread data  
typedef struct{
    int thread_id;
    int num_recipes;
} enthusiast_data_t;

//supplier thread function
void* supplier_thread(void* arg){
    supplier_data_t* data = (supplier_data_t*)arg;
    int ingredient_id = data->ingredient_id;
    int pouches_to_add = data->pouches_to_add;
    while(program_running){
        //wait until pouches reach 0 (supplier is needed)
        pthread_mutex_lock(&mutex);
        while(pouches[ingredient_id] > 0 && program_running){
            pthread_mutex_unlock(&mutex);
            sem_wait(&supplier_sem[ingredient_id]);
            pthread_mutex_lock(&mutex);
        }
        if(!program_running){
            pthread_mutex_unlock(&mutex);
            break;
        }
        //add pouches to the table
        pouches[ingredient_id] += pouches_to_add;
        printf("\033[0;31m%s supplier added %d pouches of %s to the table.\033[0m\n", 
               ingredient_names[ingredient_id], pouches_to_add, ingredient_names[ingredient_id]);
        pthread_mutex_unlock(&mutex);
    }
    printf("\033[0;31m%s supplier is done\033[0m\n", ingredient_names[ingredient_id]);
    return NULL;
}
//enthusiast thread function
void* enthusiast_thread(void* arg){
    enthusiast_data_t* data = (enthusiast_data_t*)arg;
    int thread_id = data->thread_id + 1; // 1-based indexing for output
    int num_recipes = data->num_recipes;
    for(int recipe_num = 0; recipe_num < num_recipes; recipe_num++){
        //generate random tea recipe with quantities (0-4 for each)
        int recipe[NUM_INGREDIENTS];
        //tea leaves: at least one variety, at most two varieties, 0-1 of each
        int has_green = rand() % 2;
        int has_black = rand() % 2;
        //ensure at least one tea leaf
        if(!has_green && !has_black){
            if(rand() % 2) has_green = 1;
            else has_black = 1;
        }
        recipe[GREEN_TEA] = has_green;
        recipe[BLACK_TEA] = has_black;
        //other ingredients: 1-4 pouches each, but at least one must be non-zero
        int has_ingredient = 0;
        do{
            recipe[BERGAMOT] = rand() % 5; //0-4
            recipe[HIBISCUS] = rand() % 5; //0-4
            recipe[PUFFED_RICE] = rand() % 5; //0-4
            recipe[SPICES] = rand() % 5; //0-4
            has_ingredient = recipe[BERGAMOT] || recipe[HIBISCUS] || 
                           recipe[PUFFED_RICE] || recipe[SPICES];
        }while(!has_ingredient);
        //print recipe
        printf("Tea enthusiast %d is requesting tea with these many pouches:\n", thread_id);
        printf("  %d %s, %d %s, %d %s, %d %s, %d %s, %d %s\n",
               recipe[GREEN_TEA], ingredient_names[GREEN_TEA],
               recipe[BLACK_TEA], ingredient_names[BLACK_TEA],
               recipe[BERGAMOT], ingredient_names[BERGAMOT],
               recipe[HIBISCUS], ingredient_names[HIBISCUS],
               recipe[PUFFED_RICE], ingredient_names[PUFFED_RICE],
               recipe[SPICES], ingredient_names[SPICES]);
        //procure all ingredients for this recipe
        for(int i = 0; i < NUM_INGREDIENTS; i++){
            if(recipe[i] > 0){
                pthread_mutex_lock(&mutex);
                //wait until enough pouches are available
                while(pouches[i] < recipe[i] && program_running){
                    // Wake up supplier if needed
                    if(pouches[i] == 0){
                        sem_post(&supplier_sem[i]);
                    }
                    pthread_mutex_unlock(&mutex);
                    usleep(1000); //small delay before checking again
                    pthread_mutex_lock(&mutex);
                }
                //take the required pouches
                pouches[i] -= recipe[i];
                //if pouches are now 0, wake up supplier
                if(pouches[i] == 0){
                    sem_post(&supplier_sem[i]);
                }
                pthread_mutex_unlock(&mutex);
            }
        }
        if(!program_running) break;
        //tea prepared
        printf("Tea for enthusiast %d is prepared\n", thread_id);
        //drink tea
        usleep(rand() % 5000);
    }
    printf("Tea enthusiast %d is done\n", thread_id);
    free(data);
    return NULL;
}
int main(int argc, char* argv[]){
    //seed random number generator
    srand(time(NULL));
    //display header
    printf("CS 370 - Project 5 - Tea Enthusiast Problem - Solved by: Andre Ines\n");
    //initialize mutex
    pthread_mutex_init(&mutex, NULL);
    //initialize semaphores
    for(int i = 0; i < NUM_INGREDIENTS; i++){
        sem_init(&supplier_sem[i], 0, 0);
    }
    //create supplier threads
    pthread_t supplier_threads[NUM_SUPPLIERS];
    supplier_data_t supplier_data[NUM_SUPPLIERS];
    //random pouches between 1-10 as per assignment
    for(int i = 0; i < NUM_SUPPLIERS; i++){
        supplier_data[i].ingredient_id = i;
        supplier_data[i].pouches_to_add = 1 + (rand() % 10);
        pthread_create(&supplier_threads[i], NULL, supplier_thread, &supplier_data[i]);
    }
    //create enthusiast threads
    pthread_t enthusiast_threads[NUM_ENTHUSIASTS];
    for(int i = 0; i < NUM_ENTHUSIASTS; i++){
        enthusiast_data_t* data = malloc(sizeof(enthusiast_data_t));
        data->thread_id = i;
        data->num_recipes = 15 + (rand() % 11); // 15 to 25 recipes
        pthread_create(&enthusiast_threads[i], NULL, enthusiast_thread, data);
    }
    //wait for all enthusiast threads to complete
    for(int i = 0; i < NUM_ENTHUSIASTS; i++){
        pthread_join(enthusiast_threads[i], NULL);
    }
    printf("Tea enthusiasts finished running\n");
    //signal suppliers to terminate
    program_running = false;
    for(int i = 0; i < NUM_INGREDIENTS; i++){
        sem_post(&supplier_sem[i]);
    }
    //wait for suppliers to exit
    for(int i = 0; i < NUM_SUPPLIERS; i++){
        pthread_join(supplier_threads[i], NULL);
    }
    printf("Suppliers finished running\n");
    //cleanup
    pthread_mutex_destroy(&mutex);
    for(int i = 0; i < NUM_INGREDIENTS; i++){
        sem_destroy(&supplier_sem[i]);
    }
    return 0;
}