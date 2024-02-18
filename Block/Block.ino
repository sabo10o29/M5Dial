#include <M5Dial.h>
#include <math.h>
/**
 * @brief ベクトルを表す構造体
 */
typedef struct
{
    float x;
    float y;
} Vector;
M5Canvas img(&M5Dial.Display);

// Constants
const int SCREEN_WIDTH = 200;
const int SCREEN_HEIGHT = 200;
const int BALL_RADIUS = 5;
const int BRICK_WIDTH = 20;
const int BRICK_HEIGHT = 20;
const int SIZE = 4;
const int NUM_BRICKS = SIZE * SIZE;
const int BRICK_OFFSET_X = 120 - BRICK_WIDTH * SIZE / 2;
const int BRICK_OFFSET_Y = 120 - BRICK_HEIGHT * SIZE / 2;
const int PADDLE_ARC_X = 120;
const int PADDLE_ARC_Y = 120;
const int PADDLE_ARC_R0 = 119;
const int PADDLE_ARC_R1 = 114;
const int PADDLE_ARC_ANGLE_MOVEMENT = 15;
const int DISTANCE = 2;

// Variables
int paddleX;
float ballX, ballY;
Vector directionUnitVector;
int bricks[NUM_BRICKS];
long oldDialPosition = -999;
int paddleArcAngle0;
int paddleArcAngle1;

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;
    Serial.println("プログラム開始");

    auto cfg = M5.config();
    M5Dial.begin(cfg, true, true);
    M5.Speaker.setVolume(1);
    M5Dial.Display.setBrightness(24);
    img.createSprite(240, 240);
    img.setTextDatum(5);
    img.setFont(&fonts::Font4);
    img.setTextColor(TFT_WHITE);
    img.setTextDatum(5);
    oldDialPosition = M5Dial.Encoder.read();

    // Initialize paddle position
    paddleArcAngle0 = 0;
    paddleArcAngle1 = 30;

    // Initialize ball position and speed
    ballX = 120;
    ballY = 160;
    directionUnitVector = {1 / sqrt(2), -1 / sqrt(2)}; // 右上方向

    // Initialize bricks
    for (int i = 0; i < NUM_BRICKS; i++)
    {
        bricks[i] = 1;
    }
    delay(200);
}

void loop()
{
    M5Dial.update();
    // Clear screen
    img.fillScreen(TFT_BLACK);
    img.setTextColor(TFT_WHITE);

    // Draw arc paddle
    img.drawArc(PADDLE_ARC_X, PADDLE_ARC_Y, PADDLE_ARC_R0, PADDLE_ARC_R1, paddleArcAngle0, paddleArcAngle1, TFT_WHITE);

    // Draw ball
    img.drawCircle(ballX, ballY, BALL_RADIUS, TFT_WHITE);

    // Draw bricks
    int brickX;
    int brickY;
    for (int i = 0; i < NUM_BRICKS; i++)
    {
        if (bricks[i] == 1)
        {
            brickX = (i % SIZE) * BRICK_WIDTH + BRICK_OFFSET_X;
            brickY = (i / SIZE) * BRICK_HEIGHT + BRICK_OFFSET_Y;
            img.drawRect(brickX, brickY, BRICK_WIDTH, BRICK_HEIGHT, TFT_WHITE);
        }
    }

    // Move paddle
    long newPosition = M5Dial.Encoder.read();
    if (newPosition != oldDialPosition)
    {
        if (newPosition > oldDialPosition)
        {
            paddleArcAngle0 += PADDLE_ARC_ANGLE_MOVEMENT;
            paddleArcAngle1 += PADDLE_ARC_ANGLE_MOVEMENT;
        }
        else
        {
            paddleArcAngle0 -= PADDLE_ARC_ANGLE_MOVEMENT;
            paddleArcAngle1 -= PADDLE_ARC_ANGLE_MOVEMENT;
        }
        oldDialPosition = newPosition;
        paddleArcAngle0 = paddleArcAngle0 % 360;
        paddleArcAngle1 = paddleArcAngle1 % 360;
        if (paddleArcAngle0 < 0)
        {
            // 0度から360度に変換
            paddleArcAngle0 += 360;
        }
        if (paddleArcAngle1 < 0)
        {
            // 0度から360度に変換
            paddleArcAngle1 += 360;
        }
    }

    // Move ball
    ballX += directionUnitVector.x * DISTANCE;
    ballY += directionUnitVector.y * DISTANCE;

    // Check collision with paddle
    float x = ballX - 120;
    float y = ballY - 120;
    float r = sqrt(x * x + y * y);
    float ballAngle = atan2(y, x) * (180 / M_PI);
    if (ballAngle < 0)
    {
        // 0度から360度に変換
        ballAngle += 360;
    }
    if (r >= 120 - BALL_RADIUS - PADDLE_ARC_R0 + PADDLE_ARC_R1)
    {
        Serial.println("paddleArcAngle0:" + String(paddleArcAngle0) + ", paddleArcAngle1:" + String(paddleArcAngle1) + ", ballAngle:" + String(ballAngle));
        int tmpPaddleArcAngle1 = paddleArcAngle1;
        if (tmpPaddleArcAngle1 == 0)
        {
            tmpPaddleArcAngle1 = 360;
        }
        if (paddleArcAngle0 <= ballAngle && ballAngle <= tmpPaddleArcAngle1)
        {
            Serial.println("パドルとの衝突");

            // 法線ベクトル
            Vector normalUnitVector = {-x / r, -y / r};

            // 反射ベクトルを計算
            Vector reflectionVector = calculateReflectionVector(directionUnitVector, normalUnitVector);

            r = sqrt(reflectionVector.x * reflectionVector.x + reflectionVector.y * reflectionVector.y);
            directionUnitVector.x = reflectionVector.x / r;
            directionUnitVector.y = reflectionVector.y / r;
        }
    }

    // Check collision with bricks
    for (int i = 0; i < NUM_BRICKS; i++)
    {
        if (bricks[i] == 1)
        {
            brickX = (i % SIZE) * BRICK_WIDTH + BRICK_OFFSET_X;
            brickY = (i / SIZE) * BRICK_HEIGHT + BRICK_OFFSET_Y;
            if (brickX <= ballX && ballX <= brickX + BRICK_WIDTH && brickY <= ballY && ballY <= brickY + BRICK_HEIGHT)
            {
                Serial.println("ブロックとの衝突");
                bricks[i] = 0;
                // 法線ベクトル
                Vector normalUnitVector = {-x / r, -y / r}; // 要修正　衝突した面に対して垂直方向が法線ベクトル

                // 反射ベクトルを計算
                Vector reflectionVector = calculateReflectionVector(directionUnitVector, normalUnitVector);

                r = sqrt(reflectionVector.x * reflectionVector.x + reflectionVector.y * reflectionVector.y);
                directionUnitVector.x = reflectionVector.x / r;
                directionUnitVector.y = reflectionVector.y / r;
                break;
            }
        }
    }

    // Check collision with walls
    x = ballX - 120;
    y = ballY - 120;
    r = sqrt(x * x + y * y);
    if (r >= 120 - BALL_RADIUS)
    {
        // 法線ベクトル
        Vector normalUnitVector = {-x / r, -y / r}; // 衝突点から(120,120)(円の中心)が法線ベクトル

        // 反射ベクトルを計算
        Vector reflectionVector = calculateReflectionVector(directionUnitVector, normalUnitVector);

        r = sqrt(reflectionVector.x * reflectionVector.x + reflectionVector.y * reflectionVector.y);
        directionUnitVector.x = reflectionVector.x / r;
        directionUnitVector.y = reflectionVector.y / r;
    }

    // TODO: Check game over
    if (ballY + BALL_RADIUS >= SCREEN_HEIGHT)
    {
        // TODO: Game over logic here
    }

    img.pushSprite(0, 0);
    delay(10);
}

/**
 * @brief 反射ベクトルを計算する関数
 *
 * @param incident 入射ベクトル
 * @param normal 法線ベクトル
 * @return Vector 反射ベクトル
 */
Vector calculateReflectionVector(Vector incident, Vector normal)
{
    Vector reflectionVector;

    // ベクトルの内積を計算
    float dotProduct = incident.x * normal.x + incident.y * normal.y;

    // 反射ベクトルの各成分を計算
    reflectionVector.x = incident.x + 2 * (-dotProduct) * normal.x;
    reflectionVector.y = incident.y + 2 * (-dotProduct) * normal.y;

    return reflectionVector;
}
