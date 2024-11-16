#include <iostream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <windows.h> // For system("cls"), Sleep(), and Beep()
#include <thread>    // For multithreading
#include <atomic>    // For atomic variables to manage thread safety

using namespace std;

const int WIDTH = 30;
const int HEIGHT = 20;
const char ROAD = '|';
const char CAR = 'A';
const char OBSTACLE = '#';
const char EMPTY = ' ';
const char POWERUP = '+';
const char BULLET = '|';

// Function Prototypes (Declarations)
void playCrashSound();
void playMelody(const vector<int>& melody, const vector<int>& durations);
void playPowerUpMusic();
void playGameOverSound(); // New function to play game over sound
void setColor(int textColor, int backgroundColor);
void setRandomColor(int& textColor, int& backgroundColor);
void displayLoadingScreen(); // Function to display a loading screen

// Car structure
struct Car {
    int x, y;
    bool hasSpeedBoost; // If the car has a speed boost
    int speedBoostDuration; // How long the speed boost lasts
    Car(int startX, int startY) : x(startX), y(startY), hasSpeedBoost(false), speedBoostDuration(0) {}
};

// Obstacle structure
struct Obstacle {
    int x, y;
    Obstacle(int posX, int posY) : x(posX), y(posY) {}
};

// PowerUp structure
struct PowerUp {
    int x, y;
    int type;  // 0 for Score Boost, 1 for Speed Boost
    PowerUp(int posX, int posY, int powerUpType) : x(posX), y(posY), type(powerUpType) {}
};

// Bullet structure
struct Bullet {
    int x, y;
    Bullet(int startX, int startY) : x(startX), y(startY) {}
};

// Global atomic flag for managing whether sound is playing
atomic<bool> isCrashSoundPlaying(false);
atomic<bool> isPowerUpSoundPlaying(false);

// Function to set text color (Windows-specific)
void setColor(int textColor, int backgroundColor) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (backgroundColor << 4) | textColor);
}

// Function to set random text and background colors
void setRandomColor(int& textColor, int& backgroundColor) {
    textColor = rand() % 15 + 1; // Random color for text (1 to 15)
    backgroundColor = rand() % 15 + 1; // Random color for background (1 to 15)
    setColor(textColor, backgroundColor);
}

// Move car function using GetAsyncKeyState
void moveCar(Car& car) {
    int carSpeed = 1;  // Default car speed is 1

    // If the car has a speed boost, increase its speed
    if (car.hasSpeedBoost) {
        carSpeed = 2;  // Speed boost doubles the car's movement speed
        car.speedBoostDuration--;  // Decrease the duration of the boost

        // If the speed boost has expired, reset the flag
        if (car.speedBoostDuration <= 0) {
            car.hasSpeedBoost = false;
        }
    }

    // Move left and right based on keypress, modified by car speed
    if (GetAsyncKeyState('A') & 0x8000 && car.x > 1) {
        car.x -= carSpeed; // Move left with speed boost if applicable
    }

    if (GetAsyncKeyState('D') & 0x8000 && car.x < WIDTH - 2) {
        car.x += carSpeed; // Move right with speed boost if applicable
    }
}

// Move obstacles
void moveObstacles(vector<Obstacle>& obstacles, int speed) {
    for (auto& obstacle : obstacles) {
        obstacle.y += speed; // Move obstacles faster as the game progresses
    }
}

// Generate new obstacles
void generateObstacle(vector<Obstacle>& obstacles) {
    obstacles.push_back(Obstacle(rand() % (WIDTH - 2) + 1, 1));  // Random X position
}

// Generate new power-ups
void generatePowerUp(vector<PowerUp>& powerUps) {
    if (rand() % 5 < 1) {  // Power-up generation with lower frequency
        int type = rand() % 2; // 0 = score boost, 1 = speed boost
        powerUps.push_back(PowerUp(rand() % (WIDTH - 2) + 1, 1, type));
    }
}

// Move power-ups
void movePowerUps(vector<PowerUp>& powerUps) {
    for (auto& powerUp : powerUps) {
        powerUp.y++; // Move power-up down the screen
    }
}

// Move bullets
void moveBullets(vector<Bullet>& bullets) {
    for (auto& bullet : bullets) {
        bullet.y--; // Move bullet up the screen
    }
}

// Check for collisions with obstacles
bool checkCollision(const Car& car, const vector<Obstacle>& obstacles) {
    for (const auto& obstacle : obstacles)
        if (car.x == obstacle.x && car.y == obstacle.y)
            return true;
    return false;
}

// Check if car collects a power-up
bool checkPowerUpCollection(Car& car, vector<PowerUp>& powerUps) {
    for (auto it = powerUps.begin(); it != powerUps.end(); ++it) {
        if (car.x == it->x && car.y == it->y) {
            if (it->type == 0) {
                // Score boost power-up (no changes needed)
                if (!isPowerUpSoundPlaying.load()) {
                    isPowerUpSoundPlaying.store(true);
                    thread(playPowerUpMusic).detach();
                }
            }
            else if (it->type == 1) {
                // Speed boost power-up
                car.hasSpeedBoost = true;
                car.speedBoostDuration = 10;  // Speed boost lasts for 10 frames
            }
            powerUps.erase(it);
            return true;  // Power-up collected
        }
    }
    return false;
}

// Check if bullet hits an obstacle
bool checkBulletHitObstacle(Bullet& bullet, vector<Obstacle>& obstacles, int& score) {
    for (auto it = obstacles.begin(); it != obstacles.end();) {
        if (bullet.x == it->x && bullet.y == it->y) {
            // Increase score when the bullet hits the obstacle
            score++;

            // Play crash sound if not already playing
            if (!isCrashSoundPlaying.load()) {
                isCrashSoundPlaying.store(true);
                thread(playCrashSound).detach();
            }

            // Remove the obstacle and the bullet
            it = obstacles.erase(it); // Remove the obstacle
            return true;  // Bullet hit an obstacle
        }
        else {
            ++it;
        }
    }
    return false;
}

// Check if bullet goes off the screen
bool checkBulletOffScreen(Bullet& bullet) {
    return bullet.y <= 0; // If the bullet reaches the top of the screen, it's off-screen
}

// Draw the game screen
void drawScreen(const Car& car, const vector<Obstacle>& obstacles, const vector<PowerUp>& powerUps, const vector<Bullet>& bullets, int score, int textColor, int backgroundColor) {
    system("cls");
    setColor(textColor, backgroundColor);

    // Draw the road
    for (int i = 0; i < WIDTH; ++i) cout << ROAD;
    cout << endl;

    // Draw the screen
    for (int y = 1; y < HEIGHT - 1; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            bool printed = false;

            // Draw bullets
            for (const auto& bullet : bullets) {
                if (x == bullet.x && y == bullet.y) {
                    setColor(9, backgroundColor); // Blue for bullet
                    cout << BULLET;
                    printed = true;
                    break;
                }
            }

            // Draw car
            if (x == car.x && y == car.y) {
                setColor(10, backgroundColor); // Green car
                cout << CAR;
                printed = true;
            }

            // Draw obstacles
            for (const auto& obstacle : obstacles) {
                if (x == obstacle.x && y == obstacle.y) {
                    setColor(12, backgroundColor); // Red for obstacle
                    cout << OBSTACLE;
                    printed = true;
                    break;
                }
            }

            // Draw power-ups
            for (const auto& powerUp : powerUps) {
                if (x == powerUp.x && y == powerUp.y) {
                    setColor(14, backgroundColor); // Yellow for power-up
                    cout << POWERUP;
                    printed = true;
                    break;
                }
            }

            if (!printed) {
                setColor(8, backgroundColor); // Gray for empty space
                cout << EMPTY;
            }
        }
        cout << endl;
    }

    // Draw the road bottom
    for (int i = 0; i < WIDTH; ++i) cout << ROAD;
    cout << endl;

    // Score display
    setColor(14, backgroundColor); // Yellow for score
    cout << "Score: " << score << endl;
}

// Function to play a crash sound (basic melody)
void playCrashSound() {
    Beep(523, 200); // C5 note
    Beep(587, 200); // D5 note
    Beep(659, 200); // E5 note
    Beep(698, 200); // F5 note
    isCrashSoundPlaying.store(false);
}

// Function to play a melody for power-up collection
void playPowerUpMusic() {
    vector<int> melody = { 523, 587, 659, 698 }; // C5, D5, E5, F5
    vector<int> durations = { 200, 200, 200, 200 };
    playMelody(melody, durations);
    isPowerUpSoundPlaying.store(false);
}

// Function to play a game over sound (new function)
void playGameOverSound() {
    Beep(300, 500); // Low tone for a bit of time
    Beep(250, 500); // Lower tone for a bit of time
    Beep(200, 500); // Lowest tone for a bit of time
}

// Function to play a melody (basic)
void playMelody(const vector<int>& melody, const vector<int>& durations) {
    for (size_t i = 0; i < melody.size(); ++i) {
        Beep(melody[i], durations[i]);
        this_thread::sleep_for(chrono::milliseconds(durations[i]));  // Delay between notes
    }
}

// Function to display the loading screen
void displayLoadingScreen() {
    system("cls");
    cout << "Loading Game..." << endl;
    Sleep(500);
    // Display details
    // Display some loading progress
    for (int i = 0; i <= 100; i += 10) {
        cout << "Welcome to the Turbo Rush: Road to Victory!" << endl;
        cout << "Controls: \n A - Move Left\n D - Move Right\n Space - Shoot Bullet" << endl;
        cout << "Loading: [" << string(i / 10, '=') << string(10 - i / 10, ' ') << "] " << i << "%" << endl;
        Sleep(500);
        system("cls");
    }
    cout << "\nGame Loaded! Starting..." << endl;
    Sleep(1000);
}

int main() {
    srand(time(0));

    // Display loading screen
    displayLoadingScreen();

    // Initialize game state
    Car car(WIDTH / 2, HEIGHT - 2); // Car starts at the bottom center
    vector<Obstacle> obstacles;
    vector<PowerUp> powerUps;
    vector<Bullet> bullets;
    int score = 0, p_score = 0;
    int textColor = 7, backgroundColor = 0; // Default colors (White text on Black background)

    // Game loop
    while (true) {
        // Check if the score is a multiple of 64 to change colors
        if (score % 64 == 0 && score != p_score) {
            p_score = score;
            setRandomColor(textColor, backgroundColor);
        }

        // Handle input and update game state
        moveCar(car);
        moveObstacles(obstacles, 1);
        movePowerUps(powerUps);
        moveBullets(bullets);

        // Generate new obstacles and power-ups
        if (rand() % 10 < 1) generateObstacle(obstacles);
        generatePowerUp(powerUps);

        // Check for collisions
        if (checkCollision(car, obstacles)) {
            playGameOverSound();  // Play Game Over sound when collision occurs
            break; // Game over
        }

        // Check if car collects power-ups
        checkPowerUpCollection(car, powerUps);

        // Check if bullets hit obstacles
        for (auto& bullet : bullets) {
            if (checkBulletHitObstacle(bullet, obstacles, score)) {
                bullet.y = -1; // Remove bullet after hitting obstacle
            }
        }

        // Remove bullets that go off the screen
        bullets.erase(remove_if(bullets.begin(), bullets.end(), checkBulletOffScreen), bullets.end());

        // Draw the screen
        drawScreen(car, obstacles, powerUps, bullets, score, textColor, backgroundColor);

        // Add bullet if space is pressed
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
            bullets.push_back(Bullet(car.x, car.y - 1)); // Add a bullet above the car
        }

        // Slow down the game loop a bit
        Sleep(50);
    }

    cout << "Game Over! Final Score: " << score << endl;
    Sleep(2500);
    return 0;
}
