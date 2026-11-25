#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <conio.h>
#define ROWS 15
#define COLS 30
#define MAX_BUILDINGS 150
#define MAX_TAXIS 4
#define MAX_CARS 6

struct Building {
    int x, y;
    char type;
};

struct Taxi {
    int id, x, y;
    int pickupX, pickupY;
    int destX, destY;
    int state; // 0 = free, 1 = to pickup, 2 = passenger onboard
    int rides;
    int dir;
};

struct Car {
    int x, y;
    int dir;
};

struct City {
    int day, hour, minute;
    int ticks;
    int completedRides;
    int numBuildings;
};

char city[ROWS][COLS];
struct Building buildings[MAX_BUILDINGS];
struct Taxi taxis[MAX_TAXIS];
struct Car cars[MAX_CARS];
struct City sim;

int running = 1;
int paused = 0;
int delayMs = 300;
char statusMsg[80] = "City simulation started.";

int dr[] = {-1, 1, 0, 0};
int dc[] = {0, 0, -1, 1};

void enableAnsiConsole() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode)) {
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    printf("\033[?25l\033[2J\033[H");
}

void writeLog(char message[]) {
    FILE *fp = fopen("urbix_log.txt", "a");
    if (fp == NULL) return;
    fprintf(fp, "[Day %02d %02d:%02d | Tick %d] %s\n",
            sim.day, sim.hour, sim.minute, sim.ticks, message);
    fclose(fp);
}

int isRoad(int r, int c) {
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return 0;
    return city[r][c] == '#';
}

int isVehicleAt(int r, int c) {
    int i;
    for (i = 0; i < MAX_TAXIS; i++) {
        if (taxis[i].x == r && taxis[i].y == c) return 1;
    }
    for (i = 0; i < MAX_CARS; i++) {
        if (cars[i].x == r && cars[i].y == c) return 1;
    }
    return 0;
}

int isNight() {
    return (sim.hour >= 21 || sim.hour < 6);
}

const char* getPeriod() {
    if (sim.hour >= 6 && sim.hour < 12) return "MORNING";
    if (sim.hour >= 12 && sim.hour < 17) return "AFTERNOON";
    if (sim.hour >= 17 && sim.hour < 21) return "EVENING";
    return "NIGHT";
}

const char* getTrafficLevel() {
    int active = 0, i;
    for (i = 0; i < MAX_TAXIS; i++) {
        if (taxis[i].state > 0) active++;
    }
    if (isNight()) return "LOW";
    if ((sim.hour >= 7 && sim.hour <= 10) || (sim.hour >= 17 && sim.hour <= 20) || active >= 3) {
        return "HIGH";
    }
    return "MEDIUM";
}

void generateCity() {
    int r, c, i;
    char types[] = {'H', 'S', 'O', 'P', 'B', '*'};

    sim.day = 1;
    sim.hour = 8;
    sim.minute = 0;
    sim.ticks = 0;
    sim.completedRides = 0;
    sim.numBuildings = 0;

    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++)
            city[r][c] = (r % 4 == 2 || c % 5 == 2) ? '#' : '.';
    }
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            if (city[r][c] == '.' && sim.numBuildings < MAX_BUILDINGS && (rand() % 100 < 45)) {
                char t = types[rand() % 6];
                city[r][c] = t;
                buildings[sim.numBuildings++] = (struct Building){r, c, t};
            }
        }
    }
    for (i = 0; i < MAX_TAXIS; i++) {
        do { r = rand() % ROWS; c = rand() % COLS; } while (!isRoad(r, c));
        taxis[i] = (struct Taxi){i + 1, r, c, 0, 0, 0, 0, 0, 0, rand() % 4};
    }
    for (i = 0; i < MAX_CARS; i++) {
        do { r = rand() % ROWS; c = rand() % COLS; } while (!isRoad(r, c) || isVehicleAt(r, c));
        cars[i] = (struct Car){r, c, rand() % 4};
    }

    strcpy(statusMsg, "New city generated.");
    writeLog("New city generated.");
}

void moveRandomRoad(int *x, int *y, int *dir, int avoidOthers) {
    int valid[4], count = 0, j, opp;
    opp = (*dir ^ 1); // 0<->1, 2<->3

    // Keep going straight often unless blocked or at an intersection
    if (isRoad(*x + dr[*dir], *y + dc[*dir]) &&
        (!avoidOthers || !isVehicleAt(*x + dr[*dir], *y + dc[*dir])) &&
        (rand() % 100 < 70)) {
        *x += dr[*dir];
        *y += dc[*dir];
        return;
    }

    for (j = 0; j < 4; j++) {
        int nr = *x + dr[j];
        int nc = *y + dc[j];
        if (isRoad(nr, nc) && (!avoidOthers || !isVehicleAt(nr, nc))) {
            valid[count++] = j;
        }
    }

    if (count > 0) {
        int pick = valid[rand() % count];
        if (count > 1 && pick == opp) {
            pick = valid[rand() % count];
        }
        *dir = pick;
        *x += dr[pick];
        *y += dc[pick];
    }
}

void moveToward(struct Taxi *t, int tr, int tc) {
    int r = t->x, c = t->y;
    if (r == tr && c == tc) return;

    if (r == tr && r % 4 == 2) {
        t->y += (c < tc) ? 1 : -1;
    } else if (c == tc && c % 5 == 2) {
        t->x += (r < tr) ? 1 : -1;
    } else if (r % 4 == 2) {
        if (tr % 4 == 2) {
            if (c % 5 == 2) t->x += (r < tr) ? 1 : -1;
            else t->y += (c % 5 < 2) ? 1 : -1;
        } else {
            t->y += (c < tc) ? 1 : -1;
        }
    } else {
        if (tr % 4 == 2) t->x += (r < tr) ? 1 : -1;
        else t->x += (r % 4 < 2) ? 1 : -1;
    }
}

void updateTaxis() {
    int i;
    char logMsg[80];

    for (i = 0; i < MAX_TAXIS; i++) {
        if (taxis[i].state == 0) {
            moveRandomRoad(&taxis[i].x, &taxis[i].y, &taxis[i].dir, 0);
        } else if (taxis[i].state == 1) {
            moveToward(&taxis[i], taxis[i].pickupX, taxis[i].pickupY);
            if (taxis[i].x == taxis[i].pickupX && taxis[i].y == taxis[i].pickupY) {
                taxis[i].state = 2;
                sprintf(statusMsg, "Taxi %d picked up passenger at (%d, %d).",
                        taxis[i].id, taxis[i].pickupX, taxis[i].pickupY);
                sprintf(logMsg, "Taxi %d picked up passenger at (%d, %d).",
                        taxis[i].id, taxis[i].pickupX, taxis[i].pickupY);
                writeLog(logMsg);
            }
        } else if (taxis[i].state == 2) {
            moveToward(&taxis[i], taxis[i].destX, taxis[i].destY);
            if (taxis[i].x == taxis[i].destX && taxis[i].y == taxis[i].destY) {
                taxis[i].state = 0;
                taxis[i].rides++;
                sim.completedRides++;
                sprintf(statusMsg, "Taxi %d completed ride at (%d, %d)!",
                        taxis[i].id, taxis[i].destX, taxis[i].destY);
                sprintf(logMsg, "Taxi %d completed ride to (%d, %d).",
                        taxis[i].id, taxis[i].destX, taxis[i].destY);
                writeLog(logMsg);
            }
        }
    }
}

void updateTraffic() {
    int i;
    int night = isNight();
    for (i = 0; i < MAX_CARS; i++) {
        if (night && (sim.ticks + i) % 2 == 0) continue;
        moveRandomRoad(&cars[i].x, &cars[i].y, &cars[i].dir, 1);
    }
}

void updateTime() {
    sim.ticks++;
    sim.minute += 10;
    if (sim.minute >= 60) {
        sim.minute -= 60;
        sim.hour++;
        if (sim.hour >= 24) {
            sim.hour = 0;
            sim.day++;
        }
    }
}

void printCell(int r, int c) {
    int i;
    for (i = 0; i < MAX_TAXIS; i++) {
        if (taxis[i].x == r && taxis[i].y == c) {
            printf("\033[93mT\033[0m");
            return;
        }
    }
    for (i = 0; i < MAX_CARS; i++) {
        if (cars[i].x == r && cars[i].y == c) {
            printf("\033[96mC\033[0m");
            return;
        }
    }

    char ch = city[r][c];
    int night = isNight();
    if (ch == '#') {
        printf("\033[37m#\033[0m");
    } else if (ch == 'P' || ch == '*') {
        printf("%s%c\033[0m", night ? "\033[32m" : "\033[92m", ch);
    } else if (ch == '.') {
        printf("\033[90m.\033[0m");
    } else if (night) {
        if ((r + c) % 3 == 0) printf("\033[33m%c\033[0m", ch);
        else printf("\033[90m%c\033[0m", ch);
    } else {
        if (ch == 'B') printf("\033[36mB\033[0m");
        else if (ch == 'O') printf("\033[34mO\033[0m");
        else if (ch == 'S') printf("\033[35mS\033[0m");
        else printf("\033[37mH\033[0m");
    }
}

void displayCity() {
    int r, c, i, freeTaxis = 0, activeRides = 0;
    int dispHour = sim.hour % 12;
    if (dispHour == 0) dispHour = 12;
    const char *ampm = (sim.hour < 12) ? "AM" : "PM";

    for (i = 0; i < MAX_TAXIS; i++) {
        if (taxis[i].state == 0) freeTaxis++;
        else activeRides++;
    }

    printf("\033[H");
    printf("========================================\033[K\n"
           "             URBiX 2.0\033[K\n"
           "          LIVING CITY SIM\033[K\n"
           "========================================\033[K\n");
    printf("DAY %02d | %02d:%02d %s | %-9s\033[K\n",
           sim.day, dispHour, sim.minute, ampm, getPeriod());
    printf("TRAFFIC: %-6s         STATUS: %s\033[K\n\n    ",
           getTrafficLevel(), paused ? "PAUSED " : "RUNNING");
    for (c = 0; c < COLS; c++) printf("%d", c / 10);
    printf("\033[K\n    ");
    for (c = 0; c < COLS; c++) printf("%d", c % 10);
    printf("\033[K\n");
    for (r = 0; r < ROWS; r++) {
        printf("%2d  ", r);
        for (c = 0; c < COLS; c++) printCell(r, c);
        printf("\033[K\n");
    }
    printf("\033[K\nLegend: \033[93mT\033[0m Taxi  \033[96mC\033[0m Car  # Road  \033[92mP/*\033[0m Park/Tree  H/S/O/B Bldgs\033[K\n"
           "----------------------------------------\033[K\n");
    printf("TAXIS         : %d   | FREE     : %d\033[K\n", MAX_TAXIS, freeTaxis);
    printf("ACTIVE RIDES  : %d   | COMPLETED: %d\033[K\n", activeRides, sim.completedRides);
    printf("SIMULATION    : %d TICKS (%dms)\033[K\n", sim.ticks, delayMs);
    printf("EVENT         : %s\033[K\n", statusMsg);
    printf("----------------------------------------\033[K\n"
           "[B] Book Taxi   [P] Pause/Resume\033[K\n"
           "[+] Faster      [-] Slower\033[K\n"
           "[S] Save        [L] Load\033[K\n"
           "[R] Regenerate  [Q] Quit\033[K\n"
           "========================================\033[K\n");
}

void bookTaxi() {
    int px, py, dx, dy, i, best = -1, minDist = 9999;
    char logMsg[80];

    printf("\033[?25h");
    printf("\nEnter pickup row (0-%d) and col (0-%d) on a road (#): ", ROWS - 1, COLS - 1);
    if (scanf("%d %d", &px, &py) != 2) {
        while (getchar() != '\n');
        strcpy(statusMsg, "Booking cancelled: invalid pickup input.");
        printf("\033[?25l\033[2J\033[H");
        return;
    }

    printf("Enter destination row (0-%d) and col (0-%d) on a road (#): ", ROWS - 1, COLS - 1);
    if (scanf("%d %d", &dx, &dy) != 2) {
        while (getchar() != '\n');
        strcpy(statusMsg, "Booking cancelled: invalid destination input.");
        printf("\033[?25l\033[2J\033[H");
        return;
    }

    if (!isRoad(px, py) || !isRoad(dx, dy)) {
        strcpy(statusMsg, "Booking failed: both points must be road (#) cells.");
        printf("\033[?25l\033[2J\033[H");
        return;
    }

    for (i = 0; i < MAX_TAXIS; i++) {
        if (taxis[i].state == 0) {
            int dist = abs(taxis[i].x - px) + abs(taxis[i].y - py);
            if (dist < minDist) {
                minDist = dist;
                best = i;
            }
        }
    }

    if (best == -1) {
        strcpy(statusMsg, "No free taxis available right now.");
    } else {
        taxis[best].pickupX = px;
        taxis[best].pickupY = py;
        taxis[best].destX = dx;
        taxis[best].destY = dy;
        taxis[best].state = (taxis[best].x == px && taxis[best].y == py) ? 2 : 1;
        sprintf(statusMsg, "Taxi %d booked: (%d,%d) -> (%d,%d).", taxis[best].id, px, py, dx, dy);
        sprintf(logMsg, "Taxi %d booked from (%d, %d) to (%d, %d).", taxis[best].id, px, py, dx, dy);
        writeLog(logMsg);
    }
    printf("\033[?25l\033[2J\033[H");
}

void saveCity() {
    int r, c, i;
    FILE *fp = fopen("urbix_save.txt", "w");
    if (fp == NULL) {
        strcpy(statusMsg, "Error saving to urbix_save.txt.");
        return;
    }

    fprintf(fp, "%d %d %d %d %d %d\n",
            sim.day, sim.hour, sim.minute, sim.ticks, sim.completedRides, sim.numBuildings);
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) fputc(city[r][c], fp);
        fputc('\n', fp);
    }
    for (i = 0; i < sim.numBuildings; i++) {
        fprintf(fp, "%d %d %c\n", buildings[i].x, buildings[i].y, buildings[i].type);
    }
    for (i = 0; i < MAX_TAXIS; i++) {
        fprintf(fp, "%d %d %d %d %d %d %d %d %d %d\n",
                taxis[i].id, taxis[i].x, taxis[i].y, taxis[i].pickupX, taxis[i].pickupY,
                taxis[i].destX, taxis[i].destY, taxis[i].state, taxis[i].rides, taxis[i].dir);
    }
    for (i = 0; i < MAX_CARS; i++) {
        fprintf(fp, "%d %d %d\n", cars[i].x, cars[i].y, cars[i].dir);
    }

    fclose(fp);
    strcpy(statusMsg, "Saved simulation to urbix_save.txt.");
    writeLog("Simulation saved.");
}

void loadCity() {
    int r, i;
    char line[COLS + 10];
    FILE *fp = fopen("urbix_save.txt", "r");
    if (fp == NULL) {
        strcpy(statusMsg, "No save file found (urbix_save.txt).");
        return;
    }

    if (fscanf(fp, "%d %d %d %d %d %d\n", &sim.day, &sim.hour, &sim.minute,
               &sim.ticks, &sim.completedRides, &sim.numBuildings) != 6) {
        fclose(fp);
        strcpy(statusMsg, "Invalid save file format.");
        return;
    }
    for (r = 0; r < ROWS; r++) {
        if (fgets(line, sizeof(line), fp) != NULL) strncpy(city[r], line, COLS);
    }
    for (i = 0; i < sim.numBuildings; i++) {
        fscanf(fp, "%d %d %c", &buildings[i].x, &buildings[i].y, &buildings[i].type);
    }
    for (i = 0; i < MAX_TAXIS; i++) {
        fscanf(fp, "%d %d %d %d %d %d %d %d %d %d",
               &taxis[i].id, &taxis[i].x, &taxis[i].y, &taxis[i].pickupX, &taxis[i].pickupY,
               &taxis[i].destX, &taxis[i].destY, &taxis[i].state, &taxis[i].rides, &taxis[i].dir);
    }
    for (i = 0; i < MAX_CARS; i++) {
        fscanf(fp, "%d %d %d", &cars[i].x, &cars[i].y, &cars[i].dir);
    }

    fclose(fp);
    strcpy(statusMsg, "Loaded simulation from urbix_save.txt.");
    writeLog("Simulation loaded.");
}

void checkInput() {
    if (!_kbhit()) return;
    char ch = _getch();
    if (ch >= 'a' && ch <= 'z') ch -= 32;

    switch (ch) {
        case 'B': bookTaxi(); break;
        case 'P':
            paused = !paused;
            strcpy(statusMsg, paused ? "Simulation paused." : "Simulation resumed.");
            break;
        case '+':
        case '=':
            if (delayMs > 100) delayMs -= 50;
            sprintf(statusMsg, "Speed increased (%dms).", delayMs);
            break;
        case '-':
        case '_':
            if (delayMs < 800) delayMs += 50;
            sprintf(statusMsg, "Speed decreased (%dms).", delayMs);
            break;
        case 'S': saveCity(); break;
        case 'L': loadCity(); break;
        case 'R': generateCity(); break;
        case 'Q': running = 0; break;
    }
}

int main() {
    srand((unsigned int)time(NULL));
    enableAnsiConsole();
    generateCity();

    while (running) {
        checkInput();
        if (!paused) {
            updateTaxis();
            updateTraffic();
            updateTime();
        }
        displayCity();
        Sleep(delayMs);
    }

    printf("\033[?25h\033[0m\nExited URBiX 2.0 cleanly.\n");
    return 0;
}
