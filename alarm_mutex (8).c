/*
 * alarm_mutex.c
 *
 * This is an enhancement to the alarm_thread.c program, which
 * created an "alarm thread" for each alarm command. This new
 * version uses a single alarm thread, which reads the next
 * entry in a list. The main thread places new requests onto the
 * list, in order of absolute expiration time. The list is
 * protected by a mutex, and the alarm thread sleeps for at
 * least 1 second, each iteration, to ensure that the main
 * thread can lock the mutex to add new work to the list.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;   /* seconds from EPOCH */
    char                message[128];
    int                 id;
    int                 Alarm_Time_Group_Number;    
} alarm_t;


pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;

typedef struct display_thread {
    pthread_t thread_id;
    int time_group_number;
} display_t;

display_t display_threads[100];
pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;

void* display_alarm_thread(void *arg) {
    int group_number = *(int*)arg;  // Set the group number from the passed argument
    free(arg);  // Free the dynamically allocated memory for group number

    time_t current_time;
    while (1) {
        pthread_mutex_lock(&alarm_mutex);
        for (alarm_t *alarm = alarm_list; alarm != NULL; alarm = alarm->link) {
            if (alarm->Alarm_Time_Group_Number == group_number) {
                time(&current_time);
                printf("Alarm (%d) Printed by Alarm Thread %lu for Alarm_Time_Group_Number %d at %ld: %s\n",
                       alarm->id,
                       (unsigned long)pthread_self(),
                       group_number,
                       current_time,
                       alarm->message);
            }
        }
        pthread_mutex_unlock(&alarm_mutex);
        sleep(1);
    }
    return NULL;
}

void remove_alarm(alarm_t **head, alarm_t *alarm) {
    while (*head != NULL && *head != alarm) {
        head = &(*head)->link;
    }
    if (*head != NULL) {
        *head = (*head)->link;
    }
}

//check the alarm insert the display thread
void check_and_insert(alarm_t *alarm) {

    int found = 0;
    pthread_mutex_lock(&alarm_mutex);
    pthread_mutex_lock(&display_mutex);
    for (int i = 0; i < 10; i++) {
        if (display_threads[i].time_group_number == alarm->Alarm_Time_Group_Number) {
            
            // A display thread for this group already exists
            found = 1;
            break;
        }
    }

    if (!found) {

        // Create a new display thread for this group
        for (int i = 0; i < 100; i++) {
        if (display_threads[i].time_group_number == 0) {  // 0 indicates unused slot
            int *group_number_ptr = malloc(sizeof(int));
            if (group_number_ptr == NULL) {
                fprintf(stderr, "Memory allocation failed for group_number_ptr\n");
                break; // or handle the error appropriately
            }
            *group_number_ptr = alarm->Alarm_Time_Group_Number;
            display_threads[i].time_group_number = alarm->Alarm_Time_Group_Number;

            // Create the thread
            pthread_create(&display_threads[i].thread_id, NULL, display_alarm_thread, group_number_ptr);
            
            printf("Created New Display Alarm Thread %p for Alarm_Time_Group_Number %d to Display Alarm(%d) at %ld: %s\n",
                (void*)display_threads[i].thread_id, 
                *group_number_ptr, 
                alarm->id, 
                time(NULL), 
                alarm->message);
            break;
    }
}
    }
    pthread_mutex_unlock(&alarm_mutex);
    pthread_mutex_unlock(&display_mutex);
}


/*
 * The alarm thread's start routine.
 */
void terminate_display_thread_for_group(int group_number) {
    // Implementation depends on how you manage threads
    // Example:
    for (int i = 0; i < 100; i++) {
        if (display_threads[i].time_group_number == group_number) {
            pthread_cancel(display_threads[i].thread_id); // Cancel the thread
            display_threads[i].time_group_number = 0;    // Mark as unused
            // Additional cleanup if necessary
            break;
        }
    }
}

void insert_alarm(alarm_t *alarm) {
    alarm_t **last, *next;
    int status;

    status = pthread_mutex_lock(&alarm_mutex);

    if (status != 0)
        err_abort(status, "Lock mutex");

    alarm->time = time(NULL) + alarm->seconds;  // Set the absolute time for the alarm

    // Start at the head of the list
    last = &alarm_list;
    next = *last;

    // Iterate to find the insertion point
    while (next != NULL && next->id < alarm->id) {
        last = &next->link;
        next = next->link;
    }

    // Get the current time for the insert_time
    time_t insert_time;
    time(&insert_time);

    // Get the thread ID for the main thread
    pthread_t thread_id = pthread_self();
    printf("Alarm(%d) Inserted by Main Thread %lu Into Alarm List at %ld: %s\n",
           alarm->id, 
           (unsigned long)thread_id, 
           insert_time, 
           alarm->message);
    // printf("Current head of list: %p\n", (void *)alarm_list);
    // Insert the new alarm in the list
    alarm->link = next;
    *last = alarm;
    // printf("New head of list: %p\n", (void *)alarm_list);
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0)
        err_abort(status, "Unlock mutex");

}

alarm_t* find(alarm_t *head, int id) {
    alarm_t *current = head;

    // Check if the head is NULL before accessing it
    // if (head != NULL) {
    //     printf("The head ID: %d\n", head->id);
    // } else {
    //     printf("Head is NULL\n");
    // }

    while (current != NULL) {
        if (current->id == id) {
            return current; // Alarm found, return a pointer to it
        }
        current = current->link;
    }
    return NULL; // Alarm not found
}

int has_alarms_in_group(int group_number) {
    alarm_t *current = alarm_list;
    while (current != NULL) {
        if (current->Alarm_Time_Group_Number == group_number) {
            return 1; // Alarm found in the group
        }
        current = current->link;
    }
    return 0; // No alarms found in the group
}

void *alarm_thread (void *arg) {
    alarm_t *alarm;
    int sleep_time;
    time_t now;
    int status, temp;

    while (1) {
        status = pthread_mutex_lock(&alarm_mutex);
        if (status != 0)
            err_abort(status, "Lock mutex");

        alarm = alarm_list;
        now = time(NULL);

        if (alarm == NULL) {
            sleep_time = 1;
        } else {
            if (alarm->time <= now) {
                // Time for this alarm has come
                sleep_time = 0;

                // Remove the alarm from the list
                temp = alarm->Alarm_Time_Group_Number;
                remove_alarm(&alarm_list, alarm);
                if(!has_alarms_in_group(temp)){
                    terminate_display_thread_for_group(temp);
                    printf("Display Alarm Thread for Alarm_Time_Group_Number %d Terminated at %ld\n",
                        temp, time(NULL));
                }
                // Unlock the mutex before processing the alarm to allow other threads to work
                status = pthread_mutex_unlock(&alarm_mutex);
                if (status != 0)
                    err_abort(status, "Unlock mutex");

                // Process the alarm
                printf("(%d) %s\n", alarm->seconds, alarm->message);
                free(alarm); // Assuming alarm is dynamically allocated

                continue; // Continue to the next iteration of the loop
            } else {
                // Calculate the time to sleep
                sleep_time = alarm->time - now;
            }
        }

        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0)
            err_abort(status, "Unlock mutex");
        if (sleep_time > 0)
            sleep(sleep_time);
        else
            sched_yield();
        
    }
}


void replace_alarm(int alarm_id, int seconds, const char *message) {
    alarm_t *foundAlarm, *newAlarm;
    int status, temp;
    time_t new_time = time(NULL) + seconds;

    //status = pthread_mutex_lock(&alarm_mutex);
    // if (status != 0)
    //     err_abort(status, "Lock mutex");

    foundAlarm = find(alarm_list, alarm_id);

    if (foundAlarm != NULL) {
        // Remove the existing alarm from the list
        temp = foundAlarm->Alarm_Time_Group_Number;
        remove_alarm(&alarm_list, foundAlarm);
        if(!has_alarms_in_group(temp)){
            terminate_display_thread_for_group(temp);
            printf("Display Alarm Thread for Alarm_Time_Group_Number %d Terminated at %ld\n",
                temp, time(NULL));
        }
        // Allocate and set up a new alarm
        newAlarm = (alarm_t *)malloc(sizeof(alarm_t));

        newAlarm->id = alarm_id;
        newAlarm->seconds = seconds;
        newAlarm->time = new_time;
        newAlarm->Alarm_Time_Group_Number = (seconds + 4) / 5;
        strncpy(newAlarm->message, message, sizeof(newAlarm->message) - 1);
        newAlarm->message[sizeof(newAlarm->message) - 1] = '\0';

        // Insert the new alarm into the list
        insert_alarm(newAlarm);

        // Free the memory of the old alarm, if dynamically allocated
        free(foundAlarm);

        printf("Alarm(%d) Replaced at %ld: %s\n", alarm_id, seconds, message);
    } else {
        // Handle the case where the alarm is not found
        fprintf(stderr, "Alarm ID %d not found\n", alarm_id);
    }

    // status = pthread_mutex_unlock(&alarm_mutex);
    // if (status != 0)
    //     err_abort(status, "Unlock mutex");
}



void cancel_alarm(int alarm_id) {
    alarm_t *foundAlarm;
    int status, tempGroupNumber;

    status = pthread_mutex_lock(&alarm_mutex);
    if (status != 0)
        err_abort(status, "Lock mutex");

    // Find the alarm to cancel
    foundAlarm = find(alarm_list, alarm_id);

    if (foundAlarm != NULL) {
        // Store the group number before removing the alarm
        tempGroupNumber = foundAlarm->Alarm_Time_Group_Number;

        // Remove the alarm from the list
        remove_alarm(&alarm_list, foundAlarm);

        // Free the alarm structure
        free(foundAlarm);

        // Check if any alarms are left in the removed alarm's group
        if (!has_alarms_in_group(tempGroupNumber)) {
            // Terminate the display thread for this group
            terminate_display_thread_for_group(tempGroupNumber);
            printf("Display Alarm Thread for Alarm_Time_Group_Number %d Terminated at %ld\n",
                tempGroupNumber, time(NULL));
        }
    } else {
        fprintf(stderr, "Alarm ID %d not found\n", alarm_id);
    }

    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0)
        err_abort(status, "Unlock mutex");
}

void processInput(const char *input) {
    int id, time, check;
    alarm_t *foundAlarm;
    char message[100]; // Adjust size as needed
    alarm_t *new_alarm;
    if (sscanf(input, "Replace_Alarm(%d): %d %[^\n]", &id, &time, message) == 3) {
        printf("Replace Alarm Command Detected\n");
        // printf("Alarm ID: %d, Time: %d, Message: %s\n", id, time, message);
        foundAlarm = find(alarm_list, id);
        if(foundAlarm != NULL){
            replace_alarm(id, time, message);
            printf("replace alarm sussccesful");
            check_and_insert(new_alarm);
        }
        else{
            fprintf(stderr, "Alarm ID %d not found\n", id);
        }
        
    } else if (sscanf(input, "Start_Alarm(%d): %d %[^\n]", &id, &time, message) == 3) {
        printf("Start Alarm Command Detected\n");
        // printf("Alarm ID: %d, Time: %d, Message: %s\n", id, time, message);
        new_alarm = (alarm_t *)malloc(sizeof(alarm_t));
        new_alarm->id = id;
        new_alarm->seconds = time;
        strncpy(new_alarm->message, message, sizeof(new_alarm->message));
        new_alarm->link = NULL;
        new_alarm->Alarm_Time_Group_Number = (time + 4) / 5;
        // printf("The group number is:%d\n",&new_alarm->Alarm_Time_Group_Number);
        insert_alarm(new_alarm);
        check_and_insert(new_alarm);
    }else if (sscanf(input, "Cancel_Alarm(%d)", &id) == 1) {
        printf("Cancel Alarm Command Detected\n");
        cancel_alarm(id);
    } else {
        printf("Unknown Command\n");
    }
    
 
    
// display(alarm_list);
}




int main(int argc, char *argv[]) {
    int status;
    char line[128];
    alarm_t *alarm;
    pthread_t thread;
    // Create the alarm processing thread
    status = pthread_create(&thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort(status, "Create alarm thread");
    // Main loop to read and process commands
    // alarm = (alarm_t *)malloc(sizeof(alarm_t));
    // alarm->id = 0;
    // alarm->link = NULL;
    // strcpy(alarm->message, "");
    // alarm->time = time (NULL);
    // alarm->seconds = 99999999;
    // insert_alarm(alarm);
    while (1) {
        printf("alarm> ");
        if (fgets(line, sizeof(line), stdin) == NULL) exit(0);
        if (strlen(line) <= 1) continue;
        if (strlen(line) > 128) {
            line[128] = '\0';
        }
        processInput(line);
        
    }


}

