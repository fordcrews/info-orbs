#include "ClockWidget.h"

#include "config_helper.h"

ClockWidget::ClockWidget(ScreenManager &manager) : Widget(manager) {
}

ClockWidget::~ClockWidget() {
}

void ClockWidget::setup() {
    m_lastDisplay1Digit = "";
    m_lastDisplay2Digit = "";
    m_lastDisplay4Digit = "";
    m_lastDisplay5Digit = "";

    // Schedule a glitch in 30-60 seconds
    m_nextGlitchTime = millis() + random(30000, 60000);
}

void ClockWidget::triggerRandomGlitch() {
    // Pick a random digit out of {0,1,3,4}
    int possibleDigits[4] = {0,1,3,4};
    int d = possibleDigits[random(0,4)];
    m_glitchEnabledForDigit[d] = true;
    // Next glitch in another 30-60 seconds
    m_nextGlitchTime = millis() + random(30000, 60000);
}


void ClockWidget::startDigitAnimation(int index, int oldDigit, int newDigit) {
    m_digitAnimating[index] = true;
    m_digitAnimationStart[index] = millis();
    m_digitAnimationOld[index] = oldDigit;
    m_digitAnimationNew[index] = newDigit;
    m_digitAnimationCurrent[index] = oldDigit;
    m_digitAnimationStep[index] = 0;

    // If glitch is enabled for this digit, activate glitch
    if (m_glitchEnabledForDigit[index]) {
        startGlitchForDigit(index);
    }
}

void ClockWidget::startGlitchForDigit(int index) {
    m_glitchActiveForDigit[index] = true;
    // Let's say we do 3-5 glitch steps
    m_glitchStepsRemaining[index] = random(3,6); 
}

bool ClockWidget::updateDigitAnimation(int index, unsigned long now) {
    if (!m_digitAnimating[index]) return false;

    unsigned long elapsed = now - m_digitAnimationStart[index];

    if (m_glitchActiveForDigit[index]) {
        int totalGlitchTime = (m_glitchStepsRemaining[index]) * GLITCH_STEP_DURATION;
        if (elapsed >= totalGlitchTime) {
            m_glitchActiveForDigit[index] = false;
            m_glitchEnabledForDigit[index] = false;
            m_digitAnimationStart[index] = now;
            elapsed = 0;
        } else {
            return true;
        }
    }

    int stepsPassed = elapsed / DIGIT_ANIMATION_STEP_DURATION;
    if (stepsPassed > m_digitAnimationStep[index]) {
        m_digitAnimationStep[index] = stepsPassed;

        if (m_digitAnimationStep[index] == 0) {
            // Step 0: colon_off
            m_digitAnimationCurrent[index] = -1;
        } else if (m_digitAnimationStep[index] == 1) {
            // Step 1: switch to 9
            m_digitAnimationCurrent[index] = 9;
        } else {
            // Subsequent steps: decrement by 1 or 2 (based on performance setting)
            int step = 3;  // Set to 2 to skip every other digit for faster transitions
            m_digitAnimationCurrent[index] = (m_digitAnimationCurrent[index] - step + 10) % 10;

            // Stop animation if target digit is reached
            if (m_digitAnimationCurrent[index] == m_digitAnimationNew[index]) {
                m_digitAnimating[index] = false;
            }
        }
    }

    return m_digitAnimating[index];
}


int ClockWidget::getCurrentDisplayedDigit(int index) {
    if (!m_digitAnimating[index]) {
        // Not animating: return the final digit
        switch (index) {
            case 0: return (m_hourSingle < 10 && FORMAT_24_HOUR) ? 0 : (m_hourSingle / 10);
            case 1: return (m_hourSingle % 10);
            case 3: return (m_minuteSingle < 10) ? 0 : (m_minuteSingle / 10);
            case 4: return (m_minuteSingle % 10);
        }
    } else {
        if (m_glitchActiveForDigit[index]) {
            // Show a random glitch digit
            return random(0, 10);
        } else {
            // Show the current animation step
            return m_digitAnimationCurrent[index];
        }
    }

    // Default fallback
    return 0;
}



void ClockWidget::draw(bool force) {
    m_manager.setFont(CLOCK_FONT);

    unsigned long now = millis();

    // Update animations for each digit
    bool a0 = updateDigitAnimation(0, now);
    bool a1 = updateDigitAnimation(1, now);
    bool a3 = updateDigitAnimation(3, now);
    bool a4 = updateDigitAnimation(4, now);

    // Get the current displayed digits (either glitching, animating, or normal)
    int d0 = getCurrentDisplayedDigit(0);
    int d1 = getCurrentDisplayedDigit(1);
    int d3 = getCurrentDisplayedDigit(3);
    int d4 = getCurrentDisplayedDigit(4);

    String display0 = (d0 == -1) ? "colon_off" : String(d0);
    String display1 = (d1 == -1) ? "colon_off" : String(d1);
    String display3 = (d3 == -1) ? "colon_off" : String(d3);
    String display4 = (d4 == -1) ? "colon_off" : String(d4);

    // Update the actual display if changed
    if (m_lastDisplay1Digit != display0 || force) {
        displayDigit(0, m_lastDisplay1Digit, display0, CLOCK_COLOR);
        m_lastDisplay1Digit = display0;
    }
    if (m_lastDisplay2Digit != display1 || force) {
        displayDigit(1, m_lastDisplay2Digit, display1, CLOCK_COLOR);
        m_lastDisplay2Digit = display1;
    }
    if (m_lastDisplay4Digit != display3 || force) {
        displayDigit(3, m_lastDisplay4Digit, display3, CLOCK_COLOR);
        m_lastDisplay4Digit = display3;
    }
    if (m_lastDisplay5Digit != display4 || force) {
        displayDigit(4, m_lastDisplay5Digit, display4, CLOCK_COLOR);
        m_lastDisplay5Digit = display4;
    }

    // Handle colon and seconds
    if (m_secondSingle != m_lastSecondSingle || force) {
        // Blink colon for seconds
        if (m_secondSingle % 2 == 0) {
            displayDigit(2, "", ":", CLOCK_COLOR, false);
        } else {
            displayDigit(2, "", ":", CLOCK_SHADOW_COLOR, false);
        }

        // Render second hand if enabled
#if SHOW_SECOND_TICKS == true
        displaySeconds(2, m_lastSecondSingle, TFT_BLACK);  // Erase previous position
        displaySeconds(2, m_secondSingle, CLOCK_COLOR);    // Draw at new position
#endif

        m_lastSecondSingle = m_secondSingle;
    }

    // Handle AM/PM indicator (if applicable)
    if (!FORMAT_24_HOUR && SHOW_AM_PM_INDICATOR && m_type != ClockType::NIXIE) {
        if (m_amPm != m_lastAmPm) {
            displayAmPm(m_lastAmPm, TFT_BLACK);  // Erase previous
            m_lastAmPm = m_amPm;
        }
        displayAmPm(m_amPm, CLOCK_COLOR);  // Draw current
    }
}

void ClockWidget::displayAmPm(String &amPm, uint32_t color) {
    m_manager.selectScreen(2);
    m_manager.setFontColor(color, TFT_BLACK);
    // Workaround for 12h AM/PM problem
    // The colon is slightly offset and that's a problem because to remove them, we paint over them
    // I think this is related to the TTF cache
    // The problem disappears if we reload the font here
    if (CLOCK_FONT == TTF_Font::DSEG7) {
        // We set a new font anyway
        m_manager.setFont(TTF_Font::DSEG14);
    } else {
        // Force reloading the font
        m_manager.setFont(TTF_Font::NONE);
        m_manager.setFont(CLOCK_FONT);
    }
    m_manager.drawString(amPm, SCREEN_SIZE / 5 * 4, SCREEN_SIZE / 2, 25, Align::MiddleCenter);
}

void ClockWidget::update(bool force) {
    if (millis() - m_secondTimerPrev < m_secondTimer && !force) {
        return;
    }

    GlobalTime *time = GlobalTime::getInstance();

    m_hourSingle = time->getHour();
    m_minuteSingle = time->getMinute();
    m_secondSingle = time->getSecond();
    m_amPm = time->isPM() ? "PM" : "AM";

    // Trigger a random glitch occasionally
    if (millis() > m_nextGlitchTime) {
        triggerRandomGlitch();
    }

    // Determine old digits
    int oldHourTens = (m_lastHourSingle < 10 && FORMAT_24_HOUR) ? 0 : (m_lastHourSingle / 10);
    int oldHourOnes = (m_lastHourSingle % 10);
    int oldMinTens = (m_lastMinuteSingle < 10) ? 0 : (m_lastMinuteSingle / 10);
    int oldMinOnes = (m_lastMinuteSingle % 10);

    // Determine new digits
    int newHourTens = (m_hourSingle < 10 && FORMAT_24_HOUR) ? 0 : (m_hourSingle / 10);
    int newHourOnes = (m_hourSingle % 10);
    int newMinTens = (m_minuteSingle < 10) ? 0 : (m_minuteSingle / 10);
    int newMinOnes = (m_minuteSingle % 10);

    // Check for changes and start animations if needed
    if (newHourTens != oldHourTens && !m_digitAnimating[0]) {
        startDigitAnimation(0, oldHourTens, newHourTens);
    }
    if (newHourOnes != oldHourOnes && !m_digitAnimating[1]) {
        startDigitAnimation(1, oldHourOnes, newHourOnes);
    }
    if (newMinTens != oldMinTens && !m_digitAnimating[3]) {
        startDigitAnimation(3, oldMinTens, newMinTens);
    }
    if (newMinOnes != oldMinOnes && !m_digitAnimating[4]) {
        startDigitAnimation(4, oldMinOnes, newMinOnes);
    }

    m_lastHourSingle = m_hourSingle;
    m_lastMinuteSingle = m_minuteSingle;
}

void ClockWidget::change24hMode() {
    GlobalTime *time = GlobalTime::getInstance();
    time->setFormat24Hour(!time->getFormat24Hour());
    draw(true);
}

void ClockWidget::changeClockType() {
    switch (m_type) {
    case ClockType::NORMAL:
        if (USE_CLOCK_NIXIE) {
            // If nixie is enabled, use it, otherwise fall through
            m_type = ClockType::NIXIE;
            break;
        }

    case ClockType::NIXIE:
        if (USE_CLOCK_CUSTOM) {
            // If custom is enabled, use it, otherwise fall through
            m_type = ClockType::CUSTOM;
            break;
        }

    default:
        m_type = ClockType::NORMAL;
        break;
    }
    m_manager.clearAllScreens();
    draw(true);
}

void ClockWidget::buttonPressed(uint8_t buttonId, ButtonState state) {
    if (buttonId == BUTTON_OK && state == BTN_SHORT) {
        changeClockType();
    } else if (buttonId == BUTTON_OK && state == BTN_MEDIUM) {
        change24hMode();
    }
}

DigitOffset ClockWidget::getOffsetForDigit(const String &digit) {
    if (digit.length() > 0) {
        char c = digit.charAt(0);
        if (c >= '0' && c <= '9') {
            // get digit offsets
            return m_digitOffsets[c - '0'];
        }
    }
    // not a valid digit
    return {0, 0};
}

void ClockWidget::displayDigit(int displayIndex, const String &lastDigit, const String &digit, uint32_t color, bool shadowing) {
    if (m_type == ClockType::NIXIE || m_type == ClockType::CUSTOM) {
        if (digit == ":" && color == CLOCK_SHADOW_COLOR) {
            // Show colon off
            displayImage(displayIndex, " ");
        } else {
            displayImage(displayIndex, digit);
        }
    } else {
        // Normal clock
        int fontSize = CLOCK_FONT_SIZE;
        char c = digit.charAt(0);
        bool isDigit = c >= '0' && c <= '9' || c == ' ';
        int defaultX = SCREEN_SIZE / 2 + (isDigit ? CLOCK_OFFSET_X_DIGITS : CLOCK_OFFSET_X_COLON);
        int defaultY = SCREEN_SIZE / 2;
        DigitOffset digitOffset = getOffsetForDigit(digit);
        DigitOffset lastDigitOffset = getOffsetForDigit(lastDigit);
        m_manager.selectScreen(displayIndex);
        if (shadowing) {
            m_manager.setFontColor(CLOCK_SHADOW_COLOR, TFT_BLACK);
            if (CLOCK_FONT == DSEG14) {
                // DSEG14 (from DSEGstended) uses # to fill all segments
                m_manager.drawString("#", defaultX, defaultY, fontSize, Align::MiddleCenter);
            } else if (CLOCK_FONT == DSEG7) {
                // DESG7 uses 8 to fill all segments
                m_manager.drawString("8", defaultX, defaultY, fontSize, Align::MiddleCenter);
            } else {
                // Other fonts can't be shadowed
                m_manager.setFontColor(TFT_BLACK, TFT_BLACK);
                m_manager.drawString(lastDigit, defaultX + lastDigitOffset.x, defaultY + lastDigitOffset.y, fontSize, Align::MiddleCenter);
            }
        } else {
            m_manager.setFontColor(TFT_BLACK, TFT_BLACK);
            m_manager.drawString(lastDigit, defaultX + lastDigitOffset.x, defaultY + lastDigitOffset.y, fontSize, Align::MiddleCenter);
        }
        m_manager.setFontColor(color, TFT_BLACK);
        m_manager.drawString(digit, defaultX + digitOffset.x, defaultY + digitOffset.y, fontSize, Align::MiddleCenter);
    }
}

void ClockWidget::displayDigit(int displayIndex, const String &lastDigit, const String &digit, uint32_t color) {
    displayDigit(displayIndex, lastDigit, digit, color, CLOCK_SHADOWING);
}

void ClockWidget::displaySeconds(int displayIndex, int seconds, int color) {
    if (m_type == ClockType::NIXIE && color == CLOCK_COLOR) {
        // Special color (orange) for nixie
        color = 0xfd40;
    }
    m_manager.selectScreen(displayIndex);
    if (seconds < 30) {
        m_manager.drawSmoothArc(SCREEN_SIZE / 2, SCREEN_SIZE / 2, 120, 110, 6 * seconds + 180, 6 * seconds + 180 + 6, color, TFT_BLACK);
    } else {
        m_manager.drawSmoothArc(SCREEN_SIZE / 2, SCREEN_SIZE / 2, 120, 110, 6 * seconds - 180, 6 * seconds - 180 + 6, color, TFT_BLACK);
    }
}

void ClockWidget::displayImage(int displayIndex, const String &digit) {
    switch (m_type) {
    case ClockType::NIXIE:
        displayNixie(displayIndex, digit);
        break;

    case ClockType::CUSTOM:
        displayCustom(displayIndex, digit);
        break;
    }
}

void ClockWidget::displayNixie(int displayIndex, const String &digit) {
#if USE_CLOCK_NIXIE
    if (digit.length() != 1) {
        return;
    }
    
    m_manager.selectScreen(displayIndex);
    TJpgDec.setJpgScale(1);
    switch (digit.charAt(0)) {
    case '0':
        TJpgDec.drawJpg(0, 0, nixie_0_start, nixie_0_end - nixie_0_start);
        break;
    case '1':
        TJpgDec.drawJpg(0, 0, nixie_1_start, nixie_1_end - nixie_1_start);
        break;
    case '2':
        TJpgDec.drawJpg(0, 0, nixie_2_start, nixie_2_end - nixie_2_start);
        break;
    case '3':
        TJpgDec.drawJpg(0, 0, nixie_3_start, nixie_3_end - nixie_3_start);
        break;
    case '4':
        TJpgDec.drawJpg(0, 0, nixie_4_start, nixie_4_end - nixie_4_start);
        break;
    case '5':
        TJpgDec.drawJpg(0, 0, nixie_5_start, nixie_5_end - nixie_5_start);
        break;
    case '6':
        TJpgDec.drawJpg(0, 0, nixie_6_start, nixie_6_end - nixie_6_start);
        break;
    case '7':
        TJpgDec.drawJpg(0, 0, nixie_7_start, nixie_7_end - nixie_7_start);
        break;
    case '8':
        TJpgDec.drawJpg(0, 0, nixie_8_start, nixie_8_end - nixie_8_start);
        break;
    case '9':
        TJpgDec.drawJpg(0, 0, nixie_9_start, nixie_9_end - nixie_9_start);
        break;
    case ' ':
        TJpgDec.drawJpg(0, 0, nixie_colon_off_start, nixie_colon_off_end - nixie_colon_off_start);
        break;
    case ':':
        TJpgDec.drawJpg(0, 0, nixie_colon_on_start, nixie_colon_on_end - nixie_colon_on_start);
        break;
    }
#endif
}

void ClockWidget::displayCustom(int displayIndex, const String &digit) {
#if USE_CLOCK_CUSTOM
    if (digit.length() != 1) {
        return;
    }
    m_manager.selectScreen(displayIndex);
    TJpgDec.setJpgScale(1);
    switch (digit.charAt(0)) {
    case '0':
        TJpgDec.drawJpg(0, 0, clock_custom_0_start, clock_custom_0_end - clock_custom_0_start);
        break;
    case '1':
        TJpgDec.drawJpg(0, 0, clock_custom_1_start, clock_custom_1_end - clock_custom_1_start);
        break;
    case '2':
        TJpgDec.drawJpg(0, 0, clock_custom_2_start, clock_custom_2_end - clock_custom_2_start);
        break;
    case '3':
        TJpgDec.drawJpg(0, 0, clock_custom_3_start, clock_custom_3_end - clock_custom_3_start);
        break;
    case '4':
        TJpgDec.drawJpg(0, 0, clock_custom_4_start, clock_custom_4_end - clock_custom_4_start);
        break;
    case '5':
        TJpgDec.drawJpg(0, 0, clock_custom_5_start, clock_custom_5_end - clock_custom_5_start);
        break;
    case '6':
        TJpgDec.drawJpg(0, 0, clock_custom_6_start, clock_custom_6_end - clock_custom_6_start);
        break;
    case '7':
        TJpgDec.drawJpg(0, 0, clock_custom_7_start, clock_custom_7_end - clock_custom_7_start);
        break;
    case '8':
        TJpgDec.drawJpg(0, 0, clock_custom_8_start, clock_custom_8_end - clock_custom_8_start);
        break;
    case '9':
        TJpgDec.drawJpg(0, 0, clock_custom_9_start, clock_custom_9_end - clock_custom_9_start);
        break;
    case ' ':
        TJpgDec.drawJpg(0, 0, clock_custom_colon_off_start, clock_custom_colon_off_end - clock_custom_colon_off_start);
        break;
    case ':':
        TJpgDec.drawJpg(0, 0, clock_custom_colon_on_start, clock_custom_colon_on_end - clock_custom_colon_on_start);
        break;
    }
#endif
}

String ClockWidget::getName() {
    return "Clock";
}
