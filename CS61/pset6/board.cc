#include "board.hh"
#include <mutex>


// pong_board(w, h) constructor
//    Construct a new `w x h` pong board with all empty cells.
pong_board::pong_board(int w, int h)
    : width(w), height(h), cells(w * h, pong_cell()) {
    obstacle_cell.type = cell_obstacle;
}


// pong_board destructor
pong_board::~pong_board() {
    for (auto w : this->warps) {
        delete w;
    }

    // Free dynamically allocated array
    delete[] this->marray;
}



// pong_ball::move()
//    Move this ball once.
//
//    Returns 1 if the ball successfully moved to an empty cell.
//    Returns -1 if the ball fell off the board.
//    Returns 0 otherwise. (The caller should not delay the next move if
//    this function returns 0.)
//
//    This function is complex because it must consider obstacles, collisions,
//    holes, and sticky cells. You should preserve its current logic while
//    adding sufficient synchronization to make it thread-safe.
int pong_ball::move() {

    // Scoped lock for the row above ball, row of ball, and row below ball
    std::scoped_lock scope1(this->board.marray[this->y], this->board.marray[this->y+1], this->board.marray[this->y+2]);

    pong_cell& ccur = board.cell(this->x, this->y);

    // if stopped, nothing to do
    if (this->stopped) {
        return 0;
    }

    // otherwise, ball is on board in this cell
    assert(ccur.ball == this);

    // change direction on hitting an obstacle
    pong_cell& cx = this->board.cell(this->x + this->dx, this->y);
    if (cx.type >= cell_obstacle) {
        cx.hit_obstacle();
        this->dx = -this->dx;
    }

    pong_cell& cy = this->board.cell(this->x, this->y + this->dy);
    if (cy.type >= cell_obstacle) {
        cy.hit_obstacle();
        this->dy = -this->dy;
    }

    // check next cell
    pong_cell& cnext = this->board.cell(this->x + this->dx,
                                        this->y + this->dy);

    if (cnext.ball) {
        // collision: change both balls' directions without moving them
        if (cnext.ball->dx != this->dx) {
            cnext.ball->dx = this->dx;
            this->dx = -this->dx;
        }
        if (cnext.ball->dy != this->dy) {
            cnext.ball->dy = this->dy;
            this->dy = -this->dy;
        }

        // Lock ball's mutex to changed "stopped" bool and notify ball_thread it's unstuck
        cnext.ball->ball_mutex.lock();
        cnext.ball->stopped = false;
        cnext.ball->ball_cv.notify_all();
        cnext.ball->ball_mutex.unlock();
        ++this->board.ncollisions;
        return 0;
    } else if (cnext.type == cell_warp) {
        // warp: fall off board into warp tunnel
        ccur.ball = nullptr;

        // Lock ball's mutex to change "stopped" bool and send to warp tunnel
        this->ball_mutex.lock();
        this->stopped = true;
        this->ball_mutex.unlock();
        cnext.warp->accept_ball(this);
        return 0;
    } else if (cnext.type == cell_trash) {
        // trash: kill ball
        ccur.ball = nullptr;
        return -1;
    } else if (cnext.type >= cell_obstacle) {
        // obstacle: reverse direction (but do not move)
        cnext.hit_obstacle();
        this->dx = -this->dx;
        this->dy = -this->dy;
        return 0;
    } else {
        // empty or sticky: move into it
        this->x += this->dx;
        this->y += this->dy;
        ccur.ball = nullptr;
        cnext.ball = this;
        if (cnext.type == cell_sticky) {
            // sticky: stay put until next collision
            this->dx = this->dy = 0;
            this->stopped = true;
        }
        return 1;
    }
}


// pong_cell::hit_obstacle()
//    Called from pong_ball::move when a ball hits an obstacle or paddle.

void pong_cell::hit_obstacle() {
    if (this->type == cell_obstacle
        && this->strength != 0
        && --this->strength == 0) {
        this->type = cell_empty;
    }
}


// pong_warp::accept_ball(b)
//    Called from pong_ball::move when ball `b` lands on one end of a warp
//    tunnel. Hands `b` off to that tunnel for further processing (see
//    `warp_thread` in `breakout61.cc`)
//
//    The handout code has several synchronization bugs, including that if
//    multiple balls enter a warp tunnel too close together, an assertion will
//    fail. (Instead, all balls should be accepted and then processed in
//    order.)

void pong_warp::accept_ball(pong_ball* b) {
    
    // Lock the warp's mutex to add to it's queue
    // Also notify in case queue is empty and warp_thread is blocking
    this->warp_mutex.lock();
    this->warp_vector.insert(this->warp_vector.begin(), b);
    this->warp_cv.notify_all();
    this->warp_mutex.unlock();
}
