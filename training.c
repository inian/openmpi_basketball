/* 
 * File:   main.c
 * Author: inianparameshwaran
 *
 * Created on November 6, 2012, 4:45 PM
 */
//pointer na arrow
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"

typedef struct {
    int rank;
    int pos_x_initial;
    int pos_y_initial;
    int pos_x_final;
    int pos_y_final;
    int hasball;
    int total_feet;
    int has_ball_total;
    int ball_pass_total;
    int reached;
} player;

//initializes all the players
void Initialise_players(int rank, player *p) {
    //randomly assign posistion
    p->rank = rank;
    p->pos_x_initial = rank;
    p->pos_y_initial = rank;
    p->pos_y_final = 0;
    p->pos_x_final = 0;
    p->hasball = 0;
    p->total_feet = 0;
    p->has_ball_total = 0;
    p->ball_pass_total = 0;
    p->reached = 0;
}

//initializes both the field processes
void Initialise_field(int *pos, int *field_length, int *field_width) {
    *field_length = 128;
    *field_width = 64;

    pos[0] = 64;
    pos[1] = 32;
}

/*
 * try to get the player as close to the ball as possible
 */
void movePlayer(int ball_pos[], player *p) {
    int offset_x = ball_pos[0] - p->pos_x_initial;
    int offset_y = ball_pos[1] - p->pos_y_initial;
    int total = 10;
    int feet_moved_x = 0;
    int feet_moved_y = 0;
    if (offset_x > 10) {
        feet_moved_x = 10;
    } else if (offset_x < -10) {
        feet_moved_x = -10;
    } else {
        feet_moved_x = offset_x;
    }
    total -= abs(feet_moved_x);

    //now the max the player can move is total feet in the y direction
    if (offset_y >= 10) {
        feet_moved_y = total;

    } else if (offset_y <= -10) {
        feet_moved_y = -total;
    } else {
        if (abs(offset_y) < total) {
            feet_moved_y = offset_y;
        } else {
            feet_moved_y = total;
        }
    }

    p->pos_x_final = p->pos_x_initial + feet_moved_x;
    p->pos_y_final = p->pos_y_initial + feet_moved_y;
    if (p->pos_x_final == ball_pos[0] && p->pos_y_final == ball_pos[1]) {
        p->reached = 1;
        p->hasball = 1;
        p->has_ball_total++;
    } else {
        p->hasball = 0;
        p->reached = 0;
    }
    p->total_feet += (abs(feet_moved_x) + abs(feet_moved_y));
}

//the field process with the ball resolves possible collisions between the players
void resolveCollisions(player *allplayers) {
    int i, j = -1;
    int hasball[6];
    for (i = 1; i < 6; i++) {
        if (allplayers[i].hasball == 1) {
            hasball[++j] = i;
            allplayers[i].hasball = 0;
        }
    }
    if (j == 0) {
        allplayers[hasball[0]].hasball = 1;
    } else if (j >= 1) {
        int random = (rand()) % (j + 1);
        allplayers[hasball[random]].hasball = 1;
    }
}

//the ball is thrown to some random location within the field
void throwBall(int *ball) {
    ball[0] = rand() % 128;
    ball[1] = rand() % 64;
}

int main(int argc, char** argv) {

    int size, rank, tag;
    int field_length, field_width;
    int ball_pos[2];
    int ball_pos_process[12];
    player allplayers[6]; //allplayers[0] is a dummy entry 

    //defining the new datatype
    MPI_Datatype playertype, oldtypes[1];
    int blockcounts[1];
    MPI_Aint offsets[1], extent;

    
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    srand(time(NULL) + rank);
    player p;
    offsets[0] = 0;
    oldtypes[0] = MPI_INT;
    blockcounts[0] = 1;
    MPI_Type_struct(1, blockcounts, offsets, oldtypes, &playertype);
    MPI_Type_commit(&playertype);

    Initialise_players(rank, &p);
    if (rank == 0) {
        Initialise_field(ball_pos, &field_length, &field_width);
    }

    MPI_Gather(&p, 10, playertype, allplayers, 10, playertype, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    
    //a round starts here
    //broadcast the ball position to the others
    int k;
    for (k = 0; k < 900; k++) {
        
        MPI_Bcast(ball_pos, 2, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);

        //ask the players to move around by 10 feet
        if (rank != 0) {
            movePlayer(ball_pos, &p);
        }

        MPI_Gather(&p, 10, playertype, allplayers, 10, playertype, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == 0) {
            //resolve collisions and make sure tht allplayers has only one hasball
            resolveCollisions(allplayers);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        //scatter allplayers
        MPI_Scatter(allplayers, 10, playertype, &p, 10, playertype, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        if (rank != 0) {
            ball_pos[0] = -1;
            ball_pos[1] = -1;
            //the guy with has ball one - ask him to throw the ball
            if (p.hasball == 1) {
                throwBall(ball_pos);
                p.hasball = 0;
                p.ball_pass_total++;
            }
        }

        MPI_Gather(ball_pos, 2, MPI_INT, ball_pos_process, 2, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == 0) {
            int i;
            for (i = 2; i < 12; i++) {
                if (ball_pos_process[i] > 0) {
                    ball_pos[0] = ball_pos_process[i];
                    ball_pos[1] = ball_pos_process[i + 1];
                    break;
                }
            }
        } else {
            // for the next round updating
            p.pos_x_initial = p.pos_x_final;
            p.pos_y_initial = p.pos_y_final;
        }

        MPI_Barrier(MPI_COMM_WORLD);
        if(rank == 0){
            printf("%d\n", k+1);
            printf("%d %d\n", ball_pos[0], ball_pos[1]);
            int t;
            player temp;
            for(t = 0; t < 5; t++) {
                temp = allplayers[t+1];
                printf("%d %d %d %d %d %d %d %d %d %d\n", t, temp.pos_x_initial, temp.pos_y_initial, temp.pos_x_final, temp.pos_y_final, temp.reached, temp.hasball, temp.total_feet, temp.has_ball_total, temp.ball_pass_total);
            }
        }
    } 
    MPI_Finalize();
    return (EXIT_SUCCESS);
}
