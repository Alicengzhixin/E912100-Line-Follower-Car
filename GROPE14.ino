/*
  ============================================================
  ESP32 / NodeMCU-32S 五路循跡車完整程式
  版本：加快版 + 詳細註解版
  ============================================================

  【感測器邏輯】
  白色地板 = HIGH
  黑色循跡線 = LOW

  【控制邏輯】
  黑線在左邊 → 車子往左修正
  黑線在右邊 → 車子往右修正
  黑線在中間 → 車子直走

  【馬達控制邏輯】
  正數速度 → 馬達前進
  負數速度 → 馬達後退
  速度為 0 → 主動煞車

  【循跡感測器位置】
  SENSOR_1 = 最左邊
  SENSOR_2 = 左中
  SENSOR_3 = 中間
  SENSOR_4 = 右中
  SENSOR_5 = 最右邊
*/


// ============================================================
// 1. 五路循跡感測器腳位設定
// ============================================================

// 最左邊感測器
#define SENSOR_1 36

// 左中感測器
#define SENSOR_2 39

// 中間感測器
#define SENSOR_3 34

// 右中感測器
#define SENSOR_4 35

// 最右邊感測器
#define SENSOR_5 4


/*
  感測器讀值說明：

  本車設定為：
  白色地板 → HIGH
  黑色線條 → LOW

  所以只要感測器讀到 LOW，
  就代表該感測器目前壓在黑線上。
*/
const int BLACK_SIGNAL = LOW;


// ============================================================
// 2. L298N 馬達驅動腳位設定
// ============================================================

/*
  左馬達控制腳位：

  ENA：
  左馬達 PWM 速度控制腳位。
  PWM 值越大，左馬達速度越快。

  IN1、IN2：
  左馬達方向控制腳位。

  IN1 = HIGH、IN2 = LOW → 左馬達前進
  IN1 = LOW、IN2 = HIGH → 左馬達後退
  IN1 = HIGH、IN2 = HIGH → 左馬達主動煞車
*/

// 左馬達 PWM 速度腳位
#define ENA 25

// 左馬達方向控制腳位 1
#define IN1 26

// 左馬達方向控制腳位 2
#define IN2 27


/*
  右馬達控制腳位：

  ENB：
  右馬達 PWM 速度控制腳位。

  IN3、IN4：
  右馬達方向控制腳位。

  IN3 = HIGH、IN4 = LOW → 右馬達前進
  IN3 = LOW、IN4 = HIGH → 右馬達後退
  IN3 = HIGH、IN4 = HIGH → 右馬達主動煞車
*/

// 右馬達 PWM 速度腳位
#define ENB 33

// 右馬達方向控制腳位 1
#define IN3 14

// 右馬達方向控制腳位 2
#define IN4 13


// ============================================================
// 3. 安全系統腳位設定
// ============================================================

/*
  TSMS_PIN：
  主電源安全開關。
  程式中使用 INPUT_PULLUP，因此：

  LOW  → 開關接通，允許車子運作
  HIGH → 開關未接通，車子停止

  VLS_PIN：
  起跑按鈕／VLS 訊號。
  按下後為 LOW，車子開始自主循跡。

  ESTOP_PIN：
  緊急停止按鈕。
  按下後為 LOW，立即觸發 EBS 緊急煞車。

  RESET_PIN：
  EBS 觸發後的解除按鈕。
  在 TSMS 已開啟、E-STOP 放開的情況下，
  RESET_PIN 讀到 LOW 才能解除 EBS。
*/

// 主電源安全開關
#define TSMS_PIN 32

// 起跑按鈕 / VLS 訊號
#define VLS_PIN 22

// 緊急停止按鈕
#define ESTOP_PIN 23

// EBS 重置按鈕
#define RESET_PIN 15


// ============================================================
// 4. RGB 狀態燈腳位設定
// ============================================================

/*
  RGB 燈用來顯示車子的目前狀態：

  綠燈 → SAFE_STATE：安全停止、等待狀態
  紅燈 → AUTO_STATE：自主循跡中
  藍燈 → OTHER_STATE：等待起跑 / EBS 已解除但尚未開始

  RGB_COMMON_ANODE = false：
  代表你的 RGB LED 是共陰極。

  共陰極 LED：
  HIGH → 亮
  LOW  → 滅

  若你的燈實際上是共陽極，
  請改成：

  const bool RGB_COMMON_ANODE = true;
*/

// 綠燈腳位
#define GREEN_LED 18

// 紅燈腳位
#define RED_LED 19

// 藍燈腳位
#define BLUE_LED 21

// false = 共陰極 RGB LED
const bool RGB_COMMON_ANODE = false;

// 根據共陰 / 共陽，自動決定亮燈電位
const int LED_ON = RGB_COMMON_ANODE ? LOW : HIGH;

// 根據共陰 / 共陽，自動決定熄燈電位
const int LED_OFF = RGB_COMMON_ANODE ? HIGH : LOW;


// ============================================================
// 5. 車輛狀態定義
// ============================================================

/*
  SAFE_STATE：
  車子安全停止。
  顯示綠燈。

  AUTO_STATE：
  車子正在自主循跡。
  顯示紅燈。

  OTHER_STATE：
  等待 VLS 起跑、或 EBS 已解除後等待。
  顯示藍燈。
*/

// 安全停止狀態
#define SAFE_STATE 0

// 自主循跡狀態
#define AUTO_STATE 1

// 等待 / 其他狀態
#define OTHER_STATE 2

// 儲存車子目前狀態
int carState = SAFE_STATE;


// ============================================================
// 6. PWM 設定
// ============================================================

/*
  PWM 的作用：
  用來控制馬達速度。

  PWM_FREQ = 1000：
  PWM 頻率為 1000 Hz。

  PWM_RESOLUTION = 8：
  8 位元 PWM，因此可設定範圍為：

  0   → 馬達停止
  255 → 馬達全速
*/

#define PWM_FREQ 1000
#define PWM_RESOLUTION 8


/*
  ESP32 Arduino Core 3.x 與 2.x 的 PWM 函式不同。

  以下程式會根據你安裝的 ESP32 Arduino Core 版本，
  自動選擇適合的 PWM 初始化方式。
*/


// ============================================================
// ESP32 Arduino Core 3.x PWM 寫法
// ============================================================

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3

/*
  初始化 PWM。

  ledcAttach(腳位, 頻率, 解析度)

  將 ENA 與 ENB 設為 PWM 輸出腳位。
*/
void setupPWM() {
  ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION);
}


/*
  設定左馬達 PWM 速度。

  speedValue 範圍會被限制在 0 到 255。
*/
void setLeftPWM(int speedValue) {
  speedValue = constrain(speedValue, 0, 255);
  ledcWrite(ENA, speedValue);
}


/*
  設定右馬達 PWM 速度。

  speedValue 範圍會被限制在 0 到 255。
*/
void setRightPWM(int speedValue) {
  speedValue = constrain(speedValue, 0, 255);
  ledcWrite(ENB, speedValue);
}


// ============================================================
// ESP32 Arduino Core 2.x PWM 寫法
// ============================================================

#else

/*
  Core 2.x 必須自行指定 PWM channel。

  ENA_CHANNEL → 左馬達 PWM 通道
  ENB_CHANNEL → 右馬達 PWM 通道
*/
#define ENA_CHANNEL 0
#define ENB_CHANNEL 1


/*
  初始化 PWM channel，
  並將 PWM channel 接到 ENA、ENB 腳位。
*/
void setupPWM() {
  ledcSetup(ENA_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(ENB_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(ENA, ENA_CHANNEL);
  ledcAttachPin(ENB, ENB_CHANNEL);
}


/*
  設定左馬達 PWM 速度。
*/
void setLeftPWM(int speedValue) {
  speedValue = constrain(speedValue, 0, 255);
  ledcWrite(ENA_CHANNEL, speedValue);
}


/*
  設定右馬達 PWM 速度。
*/
void setRightPWM(int speedValue) {
  speedValue = constrain(speedValue, 0, 255);
  ledcWrite(ENB_CHANNEL, speedValue);
}

#endif


// ============================================================
// 7. 速度設定：加快版
// ============================================================

/*
  這裡是最常需要調整的區域。

  PWM 範圍：
  0 到 255

  速度越高：
  車子越快，但越容易衝出去或轉彎來不及。

  速度越低：
  車子越穩，但整體完成時間會較慢。
*/


/*
  如果車子直線會衝出去：

  straightSpeed 可以先降低，例如：

  int straightSpeed = 175;

  如果找線時轉太快、容易轉過頭：

  int searchSpeed = 190;
*/


/*
  如果轉彎還是不夠快：

  mediumReverseSpeed 可以增加，例如：

  int mediumReverseSpeed = 85;

  hardReverseSpeed 可以增加，例如：

  int hardReverseSpeed = 145;
*/


/*
  直線速度。

  當黑線在中間感測器 SENSOR_3 時，
  左右馬達會以 straightSpeed 前進。
*/
int straightSpeed = 190;


/*
  小幅度修正速度。

  當車子只是稍微偏左或偏右時，
  內側馬達會較慢，外側馬達較快。

  例如黑線偏左：
  左輪使用 smallInnerSpeed
  右輪使用 smallOuterSpeed

  這樣車子會慢慢往左修正。
*/
int smallInnerSpeed = 135;
int smallOuterSpeed = 255;


/*
  中度轉彎速度。

  內側馬達會反轉，
  外側馬達則高速前進。

  這樣會讓車子的轉彎半徑變小。
*/
int mediumReverseSpeed = 70;
int mediumOuterSpeed = 255;


/*
  大轉彎速度。

  當黑線跑到最左或最右側時，
  代表車子偏離比較多。

  內側輪會更強力反轉，
  外側輪全速前進，
  讓車子快速拉回黑線。
*/
int hardReverseSpeed = 125;
int hardOuterSpeed = 255;


/*
  尋線速度。

  當五顆感測器都沒有看到黑線時，
  車子會依照 lastDirection 原地轉向找線。

  不建議設太高，
  否則容易轉過頭。
*/
int searchSpeed = 210;


// ============================================================
// 8. 安全與循跡變數
// ============================================================

/*
  lostLineStart：
  記錄「第一次完全找不到黑線」的時間。

  lostLineLimit：
  如果找不到黑線超過 5000 ms，
  也就是 5 秒，
  系統會觸發 EBS 緊急煞車。
*/
unsigned long lostLineStart = 0;
unsigned long lostLineLimit = 5000;


/*
  ebsLatched：

  false → EBS 尚未觸發
  true  → EBS 已觸發，車子鎖定停止

  EBS 觸發後，
  必須使用 RESET_PIN 才能解除。
*/
bool ebsLatched = false;


/*
  vlsStarted：

  false → 尚未按下 VLS 起跑按鈕
  true  → 已按下 VLS，允許進入自主循跡
*/
bool vlsStarted = false;


/*
  lastDirection：

   1 → 黑線最後在左邊
  -1 → 黑線最後在右邊
   0 → 黑線在中間，或尚未判斷方向

  當所有感測器都找不到黑線時，
  車子會依照這個方向原地轉向找線。

  lastDirection >= 0：
  優先向左找線。

  lastDirection < 0：
  優先向右找線。
*/
int lastDirection = 0;


/*
  DEBUG_MODE：

  true  → 在序列埠輸出感測器資料
  false → 不輸出除錯資料

  如果車子運行時覺得反應太慢，
  建議維持 false，
  因為大量 Serial.print() 可能拖慢迴圈速度。
*/
bool DEBUG_MODE = false;


// ============================================================
// 9. 函式宣告
// ============================================================

/*
  先宣告所有後面會使用的函式，
  讓 Arduino 編譯器可以先知道各函式的名稱與用途。
*/

// 根據車子狀態控制 RGB LED
void updateASL(int state);

// 停止所有馬達
void stopMotor();

// 左右馬達只前進的控制函式
void setMotorSpeed(int leftSpeed, int rightSpeed);

// 左右馬達可前進、後退或煞車的控制函式
void setMotorSigned(int leftSpeed, int rightSpeed);

// 五路循跡主控制函式
void lineFollowFastTurn();

// 找不到黑線時的尋線函式
void searchLine();

// 判斷 TSMS 主電源安全開關是否開啟
bool isTSMSOn();

// 判斷 E-STOP 是否被按下
bool isEStopPressed();

// 觸發 EBS 緊急煞車
void triggerEBS(const char* reason);


// ============================================================
// 10. setup()
// ============================================================

/*
  setup() 只會在 ESP32 開機時執行一次。

  主要工作：
  1. 開啟序列埠
  2. 設定感測器腳位
  3. 設定馬達腳位
  4. 設定 PWM
  5. 設定安全按鈕與 RGB LED
  6. 讓車子一開始保持停止
*/
void setup() {
  // 開啟序列埠，方便查看除錯資訊
  Serial.begin(115200);

  /*
    GPIO 2 通常連接到 ESP32 板載 LED。
    這裡先設為輸出並關掉。
  */
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);


  // 設定五顆循跡感測器為輸入模式
  pinMode(SENSOR_1, INPUT);
  pinMode(SENSOR_2, INPUT);
  pinMode(SENSOR_3, INPUT);
  pinMode(SENSOR_4, INPUT);
  pinMode(SENSOR_5, INPUT);


  // 設定 L298N 的方向控制腳位為輸出模式
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);


  // 初始化 PWM
  setupPWM();


  /*
    安全按鈕使用 INPUT_PULLUP。

    因此按鈕未按時：
    讀值通常為 HIGH。

    按鈕按下並接地時：
    讀值會變成 LOW。
  */
  pinMode(TSMS_PIN, INPUT_PULLUP);
  pinMode(VLS_PIN, INPUT_PULLUP);
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);


  // 設定 RGB LED 腳位為輸出
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);


  // 開機後先停止馬達
  stopMotor();

  // 開機後進入安全停止狀態，顯示綠燈
  updateASL(SAFE_STATE);

  Serial.println("ESP32 Line Following Car Ready - Faster Version");
}


// ============================================================
// 11. loop()
// ============================================================

/*
  loop() 會不停重複執行。

  程式流程：

  1. 先檢查 E-STOP
  2. 如果 EBS 已觸發，保持停止並等待 RESET
  3. 如果 TSMS 未開啟，保持停止
  4. 如果尚未按 VLS，等待起跑
  5. 如果已按 VLS，開始循跡
*/
void loop() {
  // ==========================================================
  // 11-1. E-STOP 優先檢查
  // ==========================================================

  /*
    E-STOP 為最高優先權。

    只要按下 E-STOP，
    立刻觸發 EBS。
  */
  if (isEStopPressed()) {
    triggerEBS("External emergency stop");
  }


  // ==========================================================
  // 11-2. EBS 已觸發時
  // ==========================================================

  /*
    EBS 一旦觸發後，
    車子不可以自動繼續跑。

    必須同時符合以下條件才能解除：

    1. TSMS 已開啟
    2. E-STOP 沒有按下
    3. RESET 按鈕被按下
  */
  if (ebsLatched == true) {
    // 確保馬達保持停止
    stopMotor();

    // 顯示安全停止綠燈
    carState = SAFE_STATE;
    updateASL(SAFE_STATE);

    /*
      RESET_PIN 使用 INPUT_PULLUP，
      所以按下時會讀到 LOW。
    */
    if (isTSMSOn() &&
        !isEStopPressed() &&
        digitalRead(RESET_PIN) == LOW) {

      // 解除 EBS 鎖定
      ebsLatched = false;

      // 解除 EBS 後，仍需重新按 VLS 才可起跑
      vlsStarted = false;

      // 重置丟線時間
      lostLineStart = 0;

      // 重置最後方向
      lastDirection = 0;

      // 進入等待狀態，顯示藍燈
      carState = OTHER_STATE;
      updateASL(OTHER_STATE);

      Serial.println("EBS Reset");

      // 防止按鈕彈跳
      delay(300);
    }

    // EBS 狀態下，不繼續執行下面循跡程式
    return;
  }


  // ==========================================================
  // 11-3. TSMS 未開啟時
  // ==========================================================

  /*
    如果 TSMS 安全開關未開啟，
    不論 VLS 有沒有按下，
    車子都必須停止。
  */
  if (!isTSMSOn()) {
    // 停止馬達
    stopMotor();

    // 下次要重新按 VLS 才能起跑
    vlsStarted = false;

    // 清除丟線時間
    lostLineStart = 0;

    // 清除最後方向
    lastDirection = 0;

    // 顯示安全停止綠燈
    carState = SAFE_STATE;
    updateASL(SAFE_STATE);

    delay(50);
    return;
  }


  // ==========================================================
  // 11-4. 等待 VLS 起跑
  // ==========================================================

  /*
    TSMS 已開啟，
    但尚未按下 VLS 起跑按鈕時，
    車子維持停止並顯示藍燈。
  */
  if (vlsStarted == false) {
    // 確保馬達停止
    stopMotor();

    // 顯示等待中的藍燈
    carState = OTHER_STATE;
    updateASL(OTHER_STATE);

    /*
      VLS_PIN 使用 INPUT_PULLUP，
      因此按下時讀到 LOW。
    */
    if (digitalRead(VLS_PIN) == LOW) {
      // 紀錄已開始自主循跡
      vlsStarted = true;

      // 清除丟線計時
      lostLineStart = 0;

      // 重置方向
      lastDirection = 0;

      // 顯示自主模式紅燈
      carState = AUTO_STATE;
      updateASL(AUTO_STATE);

      Serial.println("VLS pressed, start");

      // 按鈕去彈跳
      delay(300);
    }

    return;
  }


  // ==========================================================
  // 11-5. 開始循跡
  // ==========================================================

  lineFollowFastTurn();
}


// ============================================================
// 12. 五路循跡控制函式
// ============================================================

/*
  這是整份程式最核心的函式。

  工作流程：

  1. 讀取五顆感測器
  2. 判斷黑線位置
  3. 計算 error
  4. 根據 error 控制左右馬達速度
  5. 找不到黑線時進入 searchLine()
*/
void lineFollowFastTurn() {
  // 用陣列存放五顆感測器原始讀值
  int s[5];

  // 讀取每一顆感測器
  s[0] = digitalRead(SENSOR_1);
  s[1] = digitalRead(SENSOR_2);
  s[2] = digitalRead(SENSOR_3);
  s[3] = digitalRead(SENSOR_4);
  s[4] = digitalRead(SENSOR_5);


  /*
    將原始 HIGH / LOW 轉換成比較容易閱讀的布林值。

    true  → 該感測器看到黑線
    false → 該感測器沒有看到黑線
  */

  // 最左邊感測器是否看到黑線
  bool L2 = (s[0] == BLACK_SIGNAL);

  // 左中感測器是否看到黑線
  bool L1 = (s[1] == BLACK_SIGNAL);

  // 中間感測器是否看到黑線
  bool C = (s[2] == BLACK_SIGNAL);

  // 右中感測器是否看到黑線
  bool R1 = (s[3] == BLACK_SIGNAL);

  // 最右邊感測器是否看到黑線
  bool R2 = (s[4] == BLACK_SIGNAL);


  /*
    blackCount：
    用來統計目前有幾顆感測器看到黑線。

    例如：

    blackCount = 0
    代表全部感測器都沒有看到黑線。

    blackCount = 1
    代表只有一顆感測器壓到黑線。

    blackCount >= 4
    通常代表黑線很寬、路口、或多顆感測器同時壓到線。
  */
  int blackCount = 0;

  if (L2) blackCount++;
  if (L1) blackCount++;
  if (C) blackCount++;
  if (R1) blackCount++;
  if (R2) blackCount++;


  // ==========================================================
  // 除錯輸出
  // ==========================================================

  /*
    DEBUG_MODE = true 時，
    可以在 Serial Monitor 看見：

    S: 五顆感測器原始讀值
    BlackCount: 有幾顆看到黑線
    LastDir: 最後黑線方向
  */
  if (DEBUG_MODE) {
    Serial.print("S: ");
    Serial.print(s[0]);
    Serial.print(" ");
    Serial.print(s[1]);
    Serial.print(" ");
    Serial.print(s[2]);
    Serial.print(" ");
    Serial.print(s[3]);
    Serial.print(" ");
    Serial.print(s[4]);

    Serial.print("  BlackCount: ");
    Serial.print(blackCount);

    Serial.print("  LastDir: ");
    Serial.println(lastDirection);
  }


  // ==========================================================
  // 12-1. 五顆感測器都找不到黑線
  // ==========================================================

  if (blackCount == 0) {
    // 仍然是自主循跡狀態，所以維持紅燈
    carState = AUTO_STATE;
    updateASL(AUTO_STATE);

    /*
      第一次丟失黑線時，
      紀錄開始丟線的時間。
    */
    if (lostLineStart == 0) {
      lostLineStart = millis();
    }

    /*
      如果丟線時間未超過 lostLineLimit，
      先持續尋線。
    */
    if (millis() - lostLineStart <= lostLineLimit) {
      searchLine();
    } else {
      /*
        如果超過 5 秒都找不到黑線，
        認定車子失控或離開賽道，
        觸發 EBS。
      */
      triggerEBS("Line lost more than 5 seconds");
    }

    /*
      delay(0) 幾乎不會真正延遲，
      主要是讓 ESP32 系統有機會處理背景工作。
    */
    delay(0);
    return;
  }


  // ==========================================================
  // 12-2. 有重新找到黑線
  // ==========================================================

  /*
    只要至少有一顆感測器看到黑線，
    就清除丟線計時。
  */
  lostLineStart = 0;

  // 保持自主循跡紅燈
  carState = AUTO_STATE;
  updateASL(AUTO_STATE);


  // ==========================================================
  // 12-3. 計算黑線偏移量 error
  // ==========================================================

  /*
    error 用來表示黑線偏離中心的程度。

    左邊使用正數：
    L2 = +4
    L1 = +2

    中間：
    C = 0

    右邊使用負數：
    R1 = -2
    R2 = -4

    所以：

    error > 0 → 黑線在左邊 → 車子要左轉
    error < 0 → 黑線在右邊 → 車子要右轉
    error = 0 → 黑線在中間 → 車子直走
  */
  int error = 0;

  if (L2) error += 4;
  if (L1) error += 2;
  if (C) error += 0;
  if (R1) error -= 2;
  if (R2) error -= 4;


  // ==========================================================
  // 12-4. 多顆感測器同時看到黑線的處理
  // ==========================================================

  /*
    如果 blackCount >= 4，
    表示大部分感測器都壓到黑線。

    這種情況下 error 可能不可靠，
    因為左右兩邊同時都有黑線。

    所以改用 lastDirection：
    如果剛剛黑線在左邊，繼續偏左轉。
    如果剛剛黑線在右邊，繼續偏右轉。
    如果沒有方向資訊，就直走。
  */
  if (blackCount >= 4) {
    if (lastDirection > 0) {
      error = 3;
    }
    else if (lastDirection < 0) {
      error = -3;
    }
    else {
      error = 0;
    }
  }


  // ==========================================================
  // 12-5. 更新最後方向
  // ==========================================================

  /*
    將目前的 error 記錄到 lastDirection。

    這個方向主要用於：
    當車子完全找不到黑線時，
    決定要先往左找還是先往右找。
  */
  if (error > 0) {
    lastDirection = 1;
  }
  else if (error < 0) {
    lastDirection = -1;
  }


  // ==========================================================
  // 12-6. 根據 error 控制左右馬達
  // ==========================================================

  /*
    error >= 4：
    黑線在最左邊。
    使用大左轉。

    error >= 2：
    黑線偏左較多。
    使用中左轉。

    error > 0：
    黑線只偏左一點。
    使用小左修正。

    error <= -4：
    黑線在最右邊。
    使用大右轉。

    error <= -2：
    黑線偏右較多。
    使用中右轉。

    error < 0：
    黑線只偏右一點。
    使用小右修正。

    error = 0：
    黑線在中間。
    左右輪相同速度直走。
  */


  if (error >= 4) {
    // ========================================================
    // 大左轉
    // ========================================================
    /*
      左輪後退，右輪前進。

      左輪反轉速度：hardReverseSpeed
      右輪前進速度：hardOuterSpeed

      這樣車子會強力往左轉。
    */
    setMotorSigned(-hardReverseSpeed, hardOuterSpeed);
  }

  else if (error >= 2) {
    // ========================================================
    // 中左轉
    // ========================================================
    /*
      左輪小幅度反轉，
      右輪高速前進。

      比大左轉柔和，
      但比小修正轉得更快。
    */
    setMotorSigned(-mediumReverseSpeed, mediumOuterSpeed);
  }

  else if (error > 0) {
    // ========================================================
    // 小左修正
    // ========================================================
    /*
      左輪慢一點，
      右輪快一點。

      兩個輪子都前進，
      所以車子會平順地向左修正。
    */
    setMotorSigned(smallInnerSpeed, smallOuterSpeed);
  }

  else if (error <= -4) {
    // ========================================================
    // 大右轉
    // ========================================================
    /*
      左輪前進，
      右輪後退。

      車子會強力往右轉。
    */
    setMotorSigned(hardOuterSpeed, -hardReverseSpeed);
  }

  else if (error <= -2) {
    // ========================================================
    // 中右轉
    // ========================================================
    /*
      左輪高速前進，
      右輪小幅度反轉。

      車子會較快速往右轉。
    */
    setMotorSigned(mediumOuterSpeed, -mediumReverseSpeed);
  }

  else if (error < 0) {
    // ========================================================
    // 小右修正
    // ========================================================
    /*
      左輪快一點，
      右輪慢一點。

      兩輪皆前進，
      讓車子平順往右修正。
    */
    setMotorSigned(smallOuterSpeed, smallInnerSpeed);
  }

  else {
    // ========================================================
    // 直走
    // ========================================================
    /*
      黑線在中央，
      左右馬達使用一樣的速度直走。
    */
    setMotorSigned(straightSpeed, straightSpeed);
  }

  delay(0);
}


// ============================================================
// 13. 尋線模式
// ============================================================

/*
  當五顆感測器都沒有看到黑線時，
  車子會根據 lastDirection 原地轉向找線。

  lastDirection >= 0：
  預設先向左轉。

  lastDirection < 0：
  向右轉。

  注意：
  這個版本的 searchLine() 是「持續原地轉」，
  直到重新讀到黑線，或超過 5 秒觸發 EBS。
*/
void searchLine() {
  if (lastDirection >= 0) {
    /*
      向左原地轉：

      左輪後退
      右輪前進
    */
    setMotorSigned(-searchSpeed, searchSpeed);
  }
  else {
    /*
      向右原地轉：

      左輪前進
      右輪後退
    */
    setMotorSigned(searchSpeed, -searchSpeed);
  }
}


// ============================================================
// 14. TSMS 安全開關判斷
// ============================================================

/*
  TSMS_PIN 使用 INPUT_PULLUP。

  LOW  → 開關接通，允許車子運作
  HIGH → 開關未開啟，車子停止
*/
bool isTSMSOn() {
  return digitalRead(TSMS_PIN) == LOW;
}


// ============================================================
// 15. E-STOP 緊急停止判斷
// ============================================================

/*
  ESTOP_PIN 使用 INPUT_PULLUP。

  LOW → E-STOP 被按下
*/
bool isEStopPressed() {
  return digitalRead(ESTOP_PIN) == LOW;
}


// ============================================================
// 16. EBS 緊急制動
// ============================================================

/*
  triggerEBS() 會在以下情況使用：

  1. E-STOP 被按下
  2. 找不到黑線超過 5 秒

  執行後：

  1. ebsLatched 設為 true
  2. vlsStarted 設為 false
  3. 馬達停止
  4. LED 顯示綠燈
  5. 必須按 RESET 才可解除
*/
void triggerEBS(const char* reason) {
  // 鎖定 EBS 狀態
  ebsLatched = true;

  // 下次必須重新按 VLS 才能啟動
  vlsStarted = false;

  // 停止全部馬達
  stopMotor();

  // 切換成安全狀態
  carState = SAFE_STATE;
  updateASL(SAFE_STATE);

  // 輸出觸發原因到序列埠
  Serial.print("EBS Triggered: ");
  Serial.println(reason);
}


// ============================================================
// 17. ASL 狀態燈控制
// ============================================================

/*
  根據 state 控制 RGB LED。

  SAFE_STATE：
  綠燈亮。

  AUTO_STATE：
  紅燈亮。

  OTHER_STATE：
  藍燈亮。
*/
void updateASL(int state) {
  if (state == SAFE_STATE) {
    // 綠燈亮
    digitalWrite(GREEN_LED, LED_ON);
    digitalWrite(RED_LED, LED_OFF);
    digitalWrite(BLUE_LED, LED_OFF);
  }

  else if (state == AUTO_STATE) {
    // 紅燈亮
    digitalWrite(GREEN_LED, LED_OFF);
    digitalWrite(RED_LED, LED_ON);
    digitalWrite(BLUE_LED, LED_OFF);
  }

  else {
    // 藍燈亮
    digitalWrite(GREEN_LED, LED_OFF);
    digitalWrite(RED_LED, LED_OFF);
    digitalWrite(BLUE_LED, LED_ON);
  }
}


// ============================================================
// 18. 馬達控制：只前進
// ============================================================

/*
  setMotorSpeed()：

  讓左右馬達都只會前進。

  leftSpeed：
  左馬達前進速度，範圍 0 到 255。

  rightSpeed：
  右馬達前進速度，範圍 0 到 255。

  目前主循跡控制主要使用 setMotorSigned()，
  因為 setMotorSigned() 可以同時處理前進、後退與煞車。

  這個函式仍保留，
  方便你以後需要「兩輪都只前進」時使用。
*/
void setMotorSpeed(int leftSpeed, int rightSpeed) {
  // 確保速度不會低於 0 或高於 255
  leftSpeed = constrain(leftSpeed, 0, 255);
  rightSpeed = constrain(rightSpeed, 0, 255);

  // 左馬達前進
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  setLeftPWM(leftSpeed);

  // 右馬達前進
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  setRightPWM(rightSpeed);
}


// ============================================================
// 19. 馬達控制：可前進 / 後退 / 主動煞車
// ============================================================

/*
  setMotorSigned() 是主要的馬達控制函式。

  leftSpeed 與 rightSpeed 可以是：

  正數：
  馬達前進。

  負數：
  馬達後退。

  0：
  馬達主動煞車。

  範例：

  setMotorSigned(190, 190);
  → 左右輪同速前進，直走。

  setMotorSigned(120, 255);
  → 左輪慢、右輪快，車子向左轉。

  setMotorSigned(-70, 255);
  → 左輪後退、右輪前進，車子快速向左轉。

  setMotorSigned(255, -70);
  → 左輪前進、右輪後退，車子快速向右轉。
*/
void setMotorSigned(int leftSpeed, int rightSpeed) {
  // 將速度限制在 -255 到 255
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);


  // ==========================================================
  // 左馬達控制
  // ==========================================================

  if (leftSpeed > 0) {
    /*
      左馬達前進：

      IN1 = HIGH
      IN2 = LOW
    */
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);

    // PWM 值就是前進速度
    setLeftPWM(leftSpeed);
  }

  else if (leftSpeed < 0) {
    /*
      左馬達後退：

      IN1 = LOW
      IN2 = HIGH

      PWM 不能輸入負數，
      所以使用 -leftSpeed 取得正的 PWM 值。
    */
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);

    setLeftPWM(-leftSpeed);
  }

  else {
    /*
      左馬達主動煞車：

      IN1 = HIGH
      IN2 = HIGH

      兩個方向控制腳位同時 HIGH，
      讓馬達產生煞車效果。
    */
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, HIGH);

    setLeftPWM(255);
  }


  // ==========================================================
  // 右馬達控制
  // ==========================================================

  if (rightSpeed > 0) {
    /*
      右馬達前進：

      IN3 = HIGH
      IN4 = LOW
    */
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);

    setRightPWM(rightSpeed);
  }

  else if (rightSpeed < 0) {
    /*
      右馬達後退：

      IN3 = LOW
      IN4 = HIGH
    */
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);

    setRightPWM(-rightSpeed);
  }

  else {
    /*
      右馬達主動煞車：

      IN3 = HIGH
      IN4 = HIGH
    */
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, HIGH);

    setRightPWM(255);
  }
}


// ============================================================
// 20. 馬達完全停止
// ============================================================

/*
  stopMotor()：

  將左右 PWM 都設為 0，
  並把方向控制腳位設為 LOW。

  這是「停止輸出」，
  與 setMotorSigned(0, 0) 的「主動煞車」不同。

  stopMotor() 適合用於：

  1. 開機初始狀態
  2. TSMS 未開啟
  3. 等待 VLS
  4. EBS 緊急停止
*/
void stopMotor() {
  // 將兩個馬達的 PWM 歸零
  setLeftPWM(0);
  setRightPWM(0);

  // 左馬達方向腳位全部關閉
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  // 右馬達方向腳位全部關閉
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}