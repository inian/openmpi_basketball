/* 
 * File:   match.c
 * Author: inianparameshwaran
 *
 * Created on November 11, 2012, 3:42 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mpi.h"


#define FIELD_PROCESS_LEFT 0
#define FIELD_PROCESS_RIGHT 1
#define FIELD_LENGTH 128
#define FIELD_WIDTH 64
#define GOAL_LEFT_X  0
#define GOAL_LEFT_Y 32
#define GOAL_RIGHT_X 128
#define GOAL_RIGHT_Y 32
#define PLAYER_SIZE 14

typedef struct {
    int rank;
    int pos_x_initial;
    int pos_y_initial;
    int pos_x_final;
    int pos_y_final;
    int ball_target_x;
    int ball_target_y;

    int hasreachedballtotal; //total number
    int hasreached;
    int hasshot;
    int ball_challenge;

    int dribble;
    int speed;
    int shoot;
} player;

//finds the absolute distance between 2 given points
int find_distance(int pos1[2], int pos2[2]) {
    return abs(pos2[0] - pos1[0]) + abs(pos2[1] - pos1[1]);
}

//resets the ball position to the centre of the field
resetBallPosition(int ball_pos[2]) {
    ball_pos[0] = 64;
    ball_pos[1] = 32;
}

//initializing of the field done here
void Initialise_field(int rank, int ball_pos[2]) {
    resetBallPosition(ball_pos);
}

//the attributes of the players are initialized here
void Initialise_players(int rank, player* p) {
    p->ball_challenge = p->ball_target_x = p->ball_target_y = -1;
    p->shoot = 5;
    p->dribble = (5+rank)%5+1;
    p->speed = 10 - p->dribble;
    p->hasreachedballtotal = p->hasshot = 0;
    p->rank = rank;
    if (rank < 7) {
        rank = 0;
        p->pos_x_initial = p->pos_x_final = 10 * rank;
        p->pos_y_initial = p->pos_y_final = 5 * rank;
    } else {
        rank = 5;
        p->pos_x_initial = p->pos_x_final = 64 + 10 * (rank - 5);
        p->pos_y_initial = p->pos_y_final = 5 * (rank - 5);
    }
}

//the player is moved based on the ball position
void move_player(player *p, int ball_pos[2]) {
    int offset_x = ball_pos[0] - p->pos_x_initial;
    int offset_y = ball_pos[1] - p->pos_y_initial;
    int total = p->speed;
    int feet_moved_x = 0;
    int feet_moved_y = 0;
    if (offset_x >= total) {
        feet_moved_x = total;
    } else if (offset_x <= -total) {
        feet_moved_x = -total;
    } else {
        feet_moved_x = offset_x;
    }
    total -= abs(feet_moved_x);

    //now the max the player can move is total feet in the y direction
    if (offset_y >= total) {
        feet_moved_y = total;

    } else if (offset_y <= -total) {
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
}


//resolves the conflicts when multiple players are in the same position as the ball
int resolveConflicts(int* challenges, int size) {
    if (size == 0) {
        return -1;
    }
    int i, max = -1, index = -1;
    for (i = 1; i < size; i += 2) {
        if (challenges[i] > max) {
            max = challenges[i];
            index = challenges[i - 1];
        }
    }
 //since the order of the challenges in the this array cant be predicted, even if mulitple process have the same challenge it
//will b random oly

    return index;
}

void reset_array(int *arr, int size) {
    int i;
    for (i = 0; i < size; i++) {
        arr[i] = 0;
    }
}

//if the player is near goal throw it to the goal, else pass it to the nearest player nearer to the goal
void playerThrowBall(player *p, int new_ball_details[5], int half, player teamplayers[5]) {
    int goal_coord[2];
    if (p->rank < 7) {
        //team1
        if (half == 0) {
            goal_coord[0] = GOAL_RIGHT_X;
            goal_coord[1] = GOAL_RIGHT_Y;
        } else {
            goal_coord[0] = GOAL_LEFT_X;
            goal_coord[1] = GOAL_LEFT_Y;
        }
    } else {
        if (half == 0) {
            goal_coord[0] = GOAL_LEFT_X;
            goal_coord[1] = GOAL_LEFT_Y;
        } else {
            goal_coord[0] = GOAL_RIGHT_X;
            goal_coord[1] = GOAL_RIGHT_Y;
        }
    }
    p->hasshot = 1;
    new_ball_details[2] = p->shoot;
    new_ball_details[3] = p->pos_x_final;
    new_ball_details[4] = p->pos_y_final;

    int curr_pos[2];
    curr_pos[0] = p->pos_x_final;
    curr_pos[1] = p->pos_y_final;

    int i;
    //if i am 10 feet near the goal shoot
    if (find_distance(curr_pos, goal_coord) < 10) {
        new_ball_details[0] = goal_coord[0];
        new_ball_details[1] = goal_coord[1];
    } else {
        //pass the ball to the closest person other than yourself
        int x, min_distance = 200, distance, pass_pos = -1;
        int player_coord[2];
        player teammate;
        for (x = 0; x < 5; x++) {
            teammate = teamplayers[x];
            if (teammate.rank == p->rank) {
                continue;
            }
            //dont pass if the player is farther from the goal
            player_coord[0] = teammate.pos_x_final;
            player_coord[1] = teammate.pos_y_final;
            if (find_distance(player_coord, goal_coord) >= find_distance(curr_pos, goal_coord)) {
                continue;
            }
            distance = find_distance(player_coord, curr_pos);
            if (distance < min_distance) {
                min_distance = distance;
                pass_pos = x;
            }
        }
        if (pass_pos != -1) {
            new_ball_details[0] = teamplayers[pass_pos].pos_x_final;
            new_ball_details[1] = teamplayers[pass_pos].pos_y_final;
        } else {
            //directly try shooting at the goal since no1 is closer to the goal than u
            new_ball_details[0] = goal_coord[0];
            new_ball_details[1] = goal_coord[1];
        }

    }
    p->ball_target_x = new_ball_details[0];
    p->ball_target_y = new_ball_details[1];
}

//resets the position of the ball if it has gone away
void resetAwayPosition(int ball_pos[2]) {
    if(ball_pos[0] < 0) {
        ball_pos[0] = 0;
    }
    if(ball_pos[0] > FIELD_LENGTH) {
        ball_pos[0] = FIELD_LENGTH;
    }
    if(ball_pos[1] < 0) {
        ball_pos[1] = 0;
    }
    if(ball_pos[1] > FIELD_WIDTH) {
        ball_pos[1] = FIELD_WIDTH;
    }
}

//the field finalises the position of the ball based on the target ball location of the player
void finalise_ball_position(int ball_details[3], int ball_pos[2]) {
    int shoot_skill = ball_details[2];
    int distance = find_distance(ball_details, ball_pos);
    float probability = (20 + 180 * shoot_skill) / (pow(distance, 3.5) - 1);
    if (probability > 100) {
        probability = 100;
    }
    //generate a random number between 0 to 100 and if it is less than prob - score
    int random = rand() % 100;
    if (random <= probability) {
        ball_pos[0] = ball_details[0];
        ball_pos[1] = ball_details[1];
    } else {
        int offset_y = rand() % 8 + 1;
        int dir_y = rand() % 2 + 1;
        int offset_x = 8 - offset_y;
        int dir_x = rand() % 2 + 1;
        
        if (dir_x == 2) {
            offset_x *= -1;
        }

        if (dir_y == 2) {
            offset_y *= -1;
        }
        ball_pos[0] = ball_details[0] + offset_x;
        ball_pos[1] = ball_details[1] + offset_y;
        if (ball_pos[0] < 0 || ball_pos[0] > FIELD_LENGTH || ball_pos[1] < 0 || ball_pos[1] > FIELD_WIDTH) {
            resetAwayPosition(ball_pos);
        }
    }
}

//this function syncs the player information between both the field processes since some of them might have recieved dummy data
void gatherAndSyncPlayerInfo(int rank, player p, player allplayers[12], MPI_Datatype playertype) {
    MPI_Status status;
    if (rank >= 2) {
        //since the player must send its field location only to the in charge process, it sends dummy values for the position to
        //the other field process
        int temp_x = p.pos_x_final;
        int temp_y = p.pos_y_final;
        if (p.pos_x_final <= 64) {
            MPI_Send(&p, PLAYER_SIZE, playertype, FIELD_PROCESS_LEFT, 24, MPI_COMM_WORLD);
            //dummy values being sent to the wrong field
            p.pos_x_final = -1;
            p.pos_y_final = -1;
            MPI_Send(&p, PLAYER_SIZE, playertype, FIELD_PROCESS_RIGHT, 24, MPI_COMM_WORLD);
        } else {
            MPI_Send(&p, PLAYER_SIZE, playertype, FIELD_PROCESS_RIGHT, 24, MPI_COMM_WORLD);
            //dummy values being sent to the wrong field
            p.pos_x_final = -1;
            p.pos_y_final = -1;
            MPI_Send(&p, PLAYER_SIZE, playertype, FIELD_PROCESS_LEFT, 24, MPI_COMM_WORLD);
        }
        //resetting the values
        p.pos_x_final = temp_x;
        p.pos_y_final = temp_y;
    }
    if (rank < 2) {
        int i;
        player temp;
        for (i = 2; i < 12; i++) {
            MPI_Recv(&temp, PLAYER_SIZE, playertype, MPI_ANY_SOURCE, 24, MPI_COMM_WORLD, &status);
            allplayers[temp.rank] = temp;
        }
    }

    //sync both
    if (rank == 0) {
        int i;
        player temp;
        for (i = 2; i < 12; i++) {
            temp = allplayers[i];
            //check if it is a valid player
            if (temp.pos_x_final == -1 && temp.pos_y_final == -1) {
                //nt valid receive from other process
                MPI_Recv(&temp, PLAYER_SIZE, MPI_INT, FIELD_PROCESS_RIGHT, 27, MPI_COMM_WORLD, &status);
                allplayers[i] = temp;
            }
        }
    } else if (rank == 1) {
        int i;
        player temp;
        for (i = 2; i < 12; i++) {
            temp = allplayers[i];
            if (temp.pos_x_final > 64) {
                MPI_Send(&temp, PLAYER_SIZE, playertype, FIELD_PROCESS_LEFT, 27, MPI_COMM_WORLD);
            }
        }
    }
    //rank 0 has all the correct info - pass it on to rank 1
    if (rank == 0) {
        MPI_Send(allplayers, PLAYER_SIZE * 12, playertype, FIELD_PROCESS_RIGHT, 28, MPI_COMM_WORLD);
    } else if (rank == 1) {
        MPI_Recv(allplayers, PLAYER_SIZE * 12, playertype, FIELD_PROCESS_LEFT, 28, MPI_COMM_WORLD, &status);
    }
}

int main(int argc, char** argv) {

    MPI_Group orig_group, group_a, group_b;
    MPI_Comm team_a, team_b;

    int ball_pos[2];
    int ranks_a[5] = {2, 3, 4, 5, 6};
    int ranks_b[5] = {7, 8, 9, 10, 11};
    int size, rank;
    int all_ball_challenges[20] = {0};

    //defining the new datatype
    MPI_Datatype playertype, oldtypes[1];
    int blockcounts[1];
    MPI_Aint offsets[1], extent;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);


    //initialize the new datatype
    offsets[0] = 0;
    oldtypes[0] = MPI_INT;
    blockcounts[0] = 1;
    MPI_Type_struct(1, blockcounts, offsets, oldtypes, &playertype);
    MPI_Type_commit(&playertype);

    srand(time(NULL) + rank);
    player p;
    MPI_Status status;
    int ball_challenge[2]; //0 - has ball or not, 1 - the actual challenge
    int chosen, side, teama_points = 0, teamb_points = 0;
    player allplayers[12]; // 2 to 6 are players from team a and 7 to 11 are players from team b
    int goal_coords[2];
    //getting the original group
    MPI_Comm_group(MPI_COMM_WORLD, &orig_group);

    //make the 2 communicators
    MPI_Group_incl(orig_group, 5, ranks_a, &group_a);
    MPI_Comm_create(MPI_COMM_WORLD, group_a, &team_a);

    MPI_Group_incl(orig_group, 5, ranks_b, &group_b);
    MPI_Comm_create(MPI_COMM_WORLD, group_b, &team_b);

    if (rank == FIELD_PROCESS_LEFT || rank == FIELD_PROCESS_RIGHT) {
        Initialise_field(rank, ball_pos);
    } else {
        Initialise_players(rank, &p);
    }

    if (rank < 2) {
        player temp;
        int c;
        for (c = 2; c < 12; c++) {
            Initialise_players(c, &temp);
            allplayers[c] = temp;
        }
    }

    //round starts now
    //all players do a recieve for ball position
    int k, half;
    for (half = 0; half < 2; half++) {
        for (k = 0; k < 2700; k++) {
            if (rank < 2) {
                if (ball_pos[0] <= 64) {
                    side = 0;
                } else {
                    side = 1;
                }
            }

            if (rank >= 2) {
                //resetting the position
                p.pos_x_initial = p.pos_x_final;
                p.pos_y_initial = p.pos_y_final;
                MPI_Recv(ball_pos, 2, MPI_INT, MPI_ANY_SOURCE, 25, MPI_COMM_WORLD, &status);
                if (ball_pos[0] <= 64) {
                    side = 0;
                } else {
                    side = 1;
                }
            } else if (rank == side) {
                int i;
                for (i = 2; i < 12; i++) {
                    MPI_Send(ball_pos, 2, MPI_INT, i, 25, MPI_COMM_WORLD);
                }
            }

            //move the players to the ball
            if (rank >= 2) {
                move_player(&p, ball_pos);
            } else {
                reset_array(all_ball_challenges, 20);
            }
            chosen = 0;
            ball_challenge[0] = 0;
            if (rank >= 2) {
                if (p.pos_x_final == ball_pos[0] && p.pos_y_final == ball_pos[1]) {
                    ball_challenge[0] = 1;
                    ball_challenge[1] = (rand() % 9 + 1) * p.dribble;
                    p.hasreachedballtotal++;
                    p.hasreached = 1;
                    p.ball_challenge = ball_challenge[1];
                } else {
                    ball_challenge[1] = -1;
                    p.hasreached = 0;
                    p.ball_challenge = -1;
                }
            } else {
                ball_challenge[1] = -1;
            }

            //do a sync on allplayers since it will be it needed in throwball function
            gatherAndSyncPlayerInfo(rank, p, allplayers, playertype);

            if (rank >= 2) {
                int ball_challenge_details[3];
                ball_challenge_details[0] = ball_challenge[0];
                ball_challenge_details[1] = ball_challenge[1];
                ball_challenge_details[2] = rank;
                MPI_Send(ball_challenge_details, 3, MPI_INT, side, 26, MPI_COMM_WORLD);
            } else if (rank == side) {
                int i, count = -1;
                int ball_challenge_details[3];
                for (i = 0; i < 10; i++) {
                    MPI_Recv(ball_challenge_details, 3, MPI_INT, MPI_ANY_SOURCE, 26, MPI_COMM_WORLD, &status);
                    if (ball_challenge_details[0] == 1) {
                        all_ball_challenges[++count] = ball_challenge_details[2];
                        all_ball_challenges[++count] = ball_challenge_details[1];
                    }
                }

                chosen = resolveConflicts(all_ball_challenges, count + 1);
                if (chosen == -1) {
                    //if no1 gets the ball tell every1 -1
                    int j;
                    for (j = 2; j < 12; j++) {
                        MPI_Send(&chosen, 1, MPI_INT, j, 20, MPI_COMM_WORLD);
                    }
                } else {
                    int i;
                    int message;
                    for (i = 2; i < 12; i++) {
                        message = 0;
                        if (i == chosen) {
                            message = 1;
                        }
                        MPI_Send(&message, 1, MPI_INT, i, 20, MPI_COMM_WORLD);
                        //possible deadlock
                        if (message == 1) {
                            if (i < 7) {
                                player team[5];
                                int r;
                                for (r = 2; r < 7; r++) {
                                    team[r - 2] = allplayers[r];
                                }
                                MPI_Send(team, PLAYER_SIZE * 5, playertype, i, 29, MPI_COMM_WORLD);
                            } else {
                                player team[5];
                                int r;
                                for (r = 7; r < 12; r++) {
                                    team[r - 7] = allplayers[r];
                                }
                                MPI_Send(team, PLAYER_SIZE * 5, playertype, i, 29, MPI_COMM_WORLD);
                            }
                        }
                    }
                }
                MPI_Send(&chosen, 1, MPI_INT, (side + 1) % 2, 21, MPI_COMM_WORLD);
            } else if (rank == (side + 1) % 2) {
                MPI_Recv(&chosen, 1, MPI_INT, side, 21, MPI_COMM_WORLD, &status);
            }

            if (rank >= 2) {
                MPI_Recv(&chosen, 1, MPI_INT, side, 20, MPI_COMM_WORLD, &status);
                if (chosen == 1) {
                    player team[5];
                    MPI_Recv(team, 5 * PLAYER_SIZE, playertype, side, 29, MPI_COMM_WORLD, &status);
                    int new_ball_details[5];
                    new_ball_details[0] = ball_pos[0];
                    new_ball_details[1] = ball_pos[1];
                    new_ball_details[2] = p.shoot;
                    playerThrowBall(&p, new_ball_details, half, team);
                    if (ball_pos[0] <= 64) {
                        side = FIELD_PROCESS_LEFT;
                    } else {
                        side = FIELD_PROCESS_RIGHT;
                    }
                    MPI_Send(new_ball_details, 5, MPI_INT, side, 22, MPI_COMM_WORLD);
                } else {
                    p.hasshot = 0;
                    p.ball_target_x = -1;
                    p.ball_target_y = -1;
                }
            }

            //recieve the ball position, recalulate and then store it in ballpos variable if it has been updated
            if (rank < 2 && chosen != -1) {
                if (rank == side) {
                    int ball_details[5];
                    MPI_Recv(ball_details, 5, MPI_INT, MPI_ANY_SOURCE, 22, MPI_COMM_WORLD, &status);
                    finalise_ball_position(ball_details, ball_pos);
                    int shoot_coord[2];
                    shoot_coord[0] = ball_details[3];
                    shoot_coord[1] = ball_details[4];
                    if (ball_pos[0] == GOAL_LEFT_X && ball_pos[1] == GOAL_LEFT_Y) {
                        goal_coords[0] = GOAL_LEFT_X;
                        goal_coords[1] = GOAL_LEFT_Y;
                        if (half == 0) {
                            if (find_distance(shoot_coord, goal_coords) > 24) {
                                teamb_points += 3;
                            } else {
                                teamb_points += 2;
                            }
                        } else {
                            if (find_distance(shoot_coord, goal_coords) > 24) {
                                teama_points += 3;
                            } else {
                                teama_points += 2;
                            }
                        }
                        resetBallPosition(ball_pos);
                    } else if (ball_pos[0] == GOAL_RIGHT_X && ball_pos[1] == GOAL_RIGHT_Y) {
                        goal_coords[0] = GOAL_RIGHT_X;
                        goal_coords[1] = GOAL_RIGHT_Y;
                        if (half == 0) {
                            if (find_distance(goal_coords, shoot_coord) > 24) {
                                teama_points += 3;
                            } else {
                                teama_points += 2;
                            }
                        } else {
                            if (find_distance(goal_coords, shoot_coord) > 24) {
                                teamb_points += 3;
                            } else {
                                teamb_points += 2;
                            }
                        }
                        resetBallPosition(ball_pos);
                    }
                    //send the ball position to the other field process
                    int content[4];
                    content[0] = ball_pos[0];
                    content[1] = ball_pos[1];
                    content[2] = teama_points;
                    content[3] = teamb_points;
                    MPI_Send(content, 4, MPI_INT, (side + 1) % 2, 23, MPI_COMM_WORLD);
                }
                if (rank == (side + 1) % 2) {
                    int content[3];
                    MPI_Recv(content, 4, MPI_INT, side, 23, MPI_COMM_WORLD, &status);
                    ball_pos[0] = content[0]; ball_pos[1] = content[1];
                    teama_points = content[2]; teamb_points = content[3];
                }

                

            }
            gatherAndSyncPlayerInfo(rank, p, allplayers, playertype);

            //print stuff
            if (rank == 0) {
                printf("%d\n", k + 1);
                printf("%d %d\n", teama_points, teamb_points);
                printf("%d %d\n", ball_pos[0], ball_pos[1]);
                int z;
                player p1;
                for (z = 2; z < 12; z++) {
                    p1 = allplayers[z];
                    printf("%d %d %d %d %d %d %d %d %d %d\n", (z-2)%5, p1.pos_x_initial, p1.pos_y_initial, p1.pos_x_final, p1.pos_y_final, p1.hasreached, p1.hasshot, p1.ball_challenge, p1.ball_target_x, p1.ball_target_y);
                }
            }
        }
    }

    MPI_Finalize();
    return (EXIT_SUCCESS);
}