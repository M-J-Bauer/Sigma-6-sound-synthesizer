/**
 * File:       m0_synth_panel (.ino)
 *
 * Module:     User Interface for ItsyBitsy M0 Synth / Sigma-6 Voice Module 
 *             comprising IIC OLED display, 2 push-buttons and Data Entry Pot.
 *
 * Platform:   Adafruit 'ItsyBitsy M0 Express' or compatible (MCU: ATSAMD21G18)
 *
 * Author:     M.J.Bauer, 2025 -- www.mjbauer.biz
 */
#include "common_def.h"
#include "m0_synth_def.h"
#include "oled_display_lib.h"

#define BUTTON_A_PRESSED (digitalRead(BUTTON_A_PIN) == LOW)
#define BUTTON_B_PRESSED (digitalRead(BUTTON_B_PIN) == LOW)

#define BUTT_POS_A     0   // Usual X-pos of button A legend
#define BUTT_POS_B     72  // Usual X-pos of button B legend

enum User_Interface_States  // aka 'Screen identifiers'
{
  STARTUP = 0,
  CALIBRATE_CV1,
  CONFIRM_DEFAULT,
  HOME_SCREEN,
  SELECT_PRESET,
  SETUP_MENU,
  SHOW_MESSAGE,
  // ... config settings ...
  SET_AMPLD_CTRL,
  SET_VIBRATO_MODE,
  SET_PITCH_BEND,
  SET_REVERB_LEVEL,
  SET_CV_OPTIONS,
  SET_MASTER_TUNE,   
  // ... patch settings ...
  SET_LFO_DEPTH,
  SET_LFO_FREQ,
  SET_LFO_RAMP,
  SET_ENV_ATTACK,
  SET_ENV_HOLD,
  SET_ENV_DECAY,
  SET_ENV_SUSTAIN,
  SET_ENV_RELEASE,
  SET_MIXER_GAIN,
  SET_LIMITER_LVL
};

// ------------  Function prototype definitions  --------------------------------------
//
void UserState_StartupScreen();
void UserState_Calibrate_CV1();
void UserState_ConfirmDefault();
void UserState_HomeScreen();
void UserState_SelectPreset();
void UserState_SetupMenu();
void UserState_ShowUserMessage();
//
void UserState_SetAmpldControl();
void UserState_SetVibratoMode();
void UserState_SetPitchBendMode();
void UserState_SetReverbLevel();
void UserState_SetCVOptions();
void UserState_SetMasterTune();
//
void UserState_Set_LFO_FM_Depth();
void UserState_Set_LFO_Freq();
void UserState_Set_LFO_RampTime();
void UserState_Set_ENV_Attack();
void UserState_Set_ENV_Hold();
void UserState_Set_ENV_Decay();
void UserState_Set_ENV_Sustain();
void UserState_Set_ENV_Release();
void UserState_Set_MixerGain();
void UserState_Set_LimiterLevel();

/*
 * Bitmap image definition
 * Image name: sigma_6_icon_24x21, width: 24, height: 21 pixels
 */
bitmap_t sigma_6_icon_24x21[] = {
  0x00, 0x00, 0x7C, 0x00, 0x01, 0xFC, 0x00, 0x03, 0xFC, 0x00, 0x03, 0xC0, 0x00, 0x07, 0x80, 0x00,
  0x07, 0x00, 0x00, 0x07, 0x00, 0x07, 0xF7, 0xF0, 0x1F, 0xF7, 0xFC, 0x3F, 0xF7, 0xFE, 0x71, 0x87,
  0x8E, 0x71, 0xC7, 0x0F, 0xE0, 0xE7, 0x07, 0xE0, 0xE7, 0x07, 0xE0, 0xE7, 0x07, 0xE0, 0xE7, 0x07,
  0xF1, 0xE7, 0x8F, 0x71, 0xC3, 0x8E, 0x7F, 0xC3, 0xFE, 0x3F, 0x81, 0xFC, 0x0E, 0x00, 0x70
};

/*
 * Bitmap image definition
 * Image name: config_icon_7x7,  width: 7, height: 7 pixels
 */
bitmap_t config_icon_9x9[] = {
  0x14, 0x00, 0x5D, 0x00, 0x22, 0x00, 0xC1, 0x80, 0x41, 0x00, 0xC1, 0x80, 0x22, 0x00,
  0x5D, 0x00, 0x14, 0x00
};

/*
 * Bitmap image definition
 * Image name: patch_icon_7x7,  width: 7, height: 7 pixels
 */
bitmap_t patch_icon_9x9[] = {
  0x49, 0x00, 0xFF, 0x80, 0x49, 0x00, 0x49, 0x00, 0xFF, 0x80, 0x49, 0x00, 0x49, 0x00,
  0xFF, 0x80, 0x49, 0x00
};

/*
 * Bitmap image definition
 * Image name: midi_conn_icon_9x9,  width: 9, height: 9 pixels
 */
bitmap_t midi_conn_icon_9x9[] = {
  0x3E, 0x00, 0x77, 0x00, 0xDD, 0x80, 0xFF, 0x80, 0xBE, 0x80,
  0xFF, 0x80, 0xFF, 0x80, 0x7F, 0x00, 0x3E, 0x00
};

char messageLine1[32], messageLine2[32];  // message to display

const uint8  percentLogScale[] =       // 16 values, 3dB log scale (approx.)
        { 0, 1, 2, 3, 4, 5, 8, 10, 12, 16, 25, 35, 50, 70, 100, 100  };

const uint16 timeValueQuantized[] =     // 16 values, logarithmic scale
        { 0, 10, 20, 30, 50, 70, 100, 200, 300, 500, 700, 1000, 1500, 2000, 3000, 5000 };


//===================   P U S H - B U T T O N   F U N C T I O N S  ======================
//
static bool buttonHit_A;    // flag: button 'A' hit detected
static bool buttonHit_B;    // flag: button 'B' hit detected
static bool buttonState_A;  // Button state, de-glitched (0: released, 1: pressed)
static bool buttonState_B;  // Button state, de-glitched (0: released, 1: pressed)
static uint32 buttonPressTime_A;  // Time button A held pressed (ms)
static uint32 buttonPressTime_B;  // Time button B held pressed (ms)
//
/*
  |  Routine to monitor button states and to detect button hits.
  |  Background task called at 5ms intervals.
  |
  |  If button # hit is detected, flag isButtonHit_# is set TRUE. (# = A or B.)
  |  While button # is held pressed, variable m_ButtonPressTime_# increments in ms.
  |  While button # is released, m_ButtonPressTime_# will be zero (0).
 */
void ButtonScan() 
{
  static uint32 buttonReleaseTime_A, buttonReleaseTime_B;
  static uint8 buttonStateLastScan_A, buttonStateLastScan_B;  // 0: Released, 1: Pressed

  if (BUTTON_A_PRESSED)  { buttonPressTime_A += 5;  buttonReleaseTime_A = 0; } 
  else  { buttonReleaseTime_A += 5;  buttonPressTime_A = 0; }  // released
  // Apply de-glitch / de-bounce filter
  if (buttonPressTime_A > 49)  buttonState_A = 1;
  if (buttonReleaseTime_A > 99)  buttonState_A = 0;
  if (buttonState_A == 1 && buttonStateLastScan_A == 0) buttonHit_A = TRUE;
 
  if (BUTTON_B_PRESSED)  { buttonPressTime_B += 5;  buttonReleaseTime_B = 0; } 
  else  { buttonReleaseTime_B += 5;  buttonPressTime_B = 0; }  // released
  // Apply de-glitch / de-bounce filter
  if (buttonPressTime_B > 49)  buttonState_B = 1;
  if (buttonReleaseTime_B > 99)  buttonState_B = 0;
  if (buttonState_B == 1 && buttonStateLastScan_B == 0) buttonHit_B = TRUE;

  buttonStateLastScan_A = buttonState_A;
  buttonStateLastScan_B = buttonState_B;
}

/*
 * Function:     BOOL ButtonHit()
 *
 * Overview:     Tests for a button hit, i.e. transition from not pressed to pressed.
 *               A private flag, m_ButtonHit_#, is cleared on exit so that the
 *               function will return TRUE once only for each button press event.
 *
 * Entry arg:    keycode = 'A' or 'B' ... identifies which button is checked
 *
 * Return val:   TRUE if a button hit was detected since the last call, else FALSE (0).
 */
BOOL ButtonHit(char keycode) 
{
  BOOL result = FALSE;

  if (keycode == 'A') 
  {
    result = buttonHit_A;
    buttonHit_A = FALSE;
  } 
  else if (keycode == 'B') 
  {
    result = buttonHit_B;
    buttonHit_B = FALSE;
  }

  return result;
}


//=================  D A T A   E N T R Y   P O T   F U N C T I O N S  ===================
//
static uint32 potReadingAve;  // smoothed pot reading [24:8 fixed-pt]
//
/*
 * Function:  PotService()
 *
 * Overview:  Service Routine for front-panel data-entry pot.
 *            Non-blocking "task" called at 5ms intervals from main loop.
 *
 * Detail:    The routine reads the pot input and keeps a rolling average of the ADC
 *            readings in fixed-point format (24:8 bits);  range 0.0 ~ 4095.995
 *            The current pot position can be read by a call to function PotPosition().
 */
void PotService() 
{
  long potReading = analogRead(A0);  // 12-bit reading;

  // Apply 1st-order IIR filter (K = 0.25)
  potReading = potReading << 8;  // convert to fixed-point (24:8 bits)
  potReadingAve -= (potReadingAve >> 2);
  potReadingAve += (potReading >> 2);
}

/*
 * Function:     PotMoved()
 *
 * Overview:     Returns TRUE if the pot position has changed by more than 2% since a
 *               previous call to the function which also returned TRUE. (80 / 4096 -> 2%)
 *
 * Return val:   (bool) status flag, value = TRUE or FALSE
 */
bool PotMoved() 
{
  static long lastReading;
  bool result = FALSE;

  if (labs(potReadingAve - lastReading) > (80 << 8)) 
  {
    result = TRUE;
    lastReading = potReadingAve;
  }
  return result;
}

/*
 * Function:     PotPosition()
 *
 * Overview:     Returns the current position of the data-entry pot, averaged over
 *               several ADC readings, as an 8-bit unsigned integer.
 *               Full-scale ADC reading is 4095.  Divide by 16 to get 255.
 *               Shift right 8 bits to get integer part; then shift 4 bits more.
 *
 * Return val:   (uint8) Filtered Pot position reading, range 0..255.
 */
uint8 PotPosition() 
{
  return (uint8)(potReadingAve >> 12);    // (Integer part) / 16
}


//=============  S C R E E N   N A V I G A T I O N   F U N C T I O N S  =================
//
static bool screenSwitchReq;      // flag: Request to switch to next screen
static bool isNewScreen;          // flag: Screen switch has occurred
static uint8 currentScreen;       // ID number of current screen displayed
static uint8 previousScreen;      // ID number of previous screen displayed
static uint8 nextScreen;          // ID number of next screen to be displayed
//
/*
 * Display title bar text left-justified top of screen in 12pt font with underline.
 * The title bar area is assumed to be already erased.
 * Maximum length of titleString is 16 ~ 17 chars.
 */
void DisplayTitleBar(const char *titleString) 
{
  Disp_Mode(SET_PIXELS);
  Disp_PosXY(4, 0);
  Disp_SetFont(PROP_12_NORM);
  Disp_PutText(titleString);
  Disp_PosXY(0, 12);
  Disp_DrawLineHoriz(128);
}

/*
 * Display a text string (8 chars max) centred in a button field of fixed width (56 px)
 * using 8pt mono-spaced font, at the specified screen position (x, y) = bar upper LHS.
 * On exit, the display write mode is restored to 'SET_PIXELS'.
 */
void DisplayButtonLegend(uint8 x, const char *str) 
{
  int len = strlen(str);
  int i, wpix;

  if (len > 8) len = 8;
  wpix = len * 6;  // text width, pixels

  Disp_Mode(SET_PIXELS);  // Draw the button (bar)
  Disp_PosXY(x, 53);
  Disp_DrawBar(56, 9);
  Disp_Mode(CLEAR_PIXELS);  // Write the string
  Disp_SetFont(MONO_8_NORM);
  x = x + (56 - wpix) / 2;
  Disp_PosXY(x, 54);
  Disp_PutText(str);
  // for (i = 0; i < len; i++) { Disp_PutChar(*str++); }
  Disp_Mode(SET_PIXELS);
}

/*
 * Function returns the screen ID number of the currently displayed screen.
 */
uint8 GetCurrentScreenID() 
{
  return currentScreen;
}

/*
 * Function GoToNextScreen() triggers a screen switch to a specified screen ID.
 * The real screen switch business is done by the UserInterfaceTask() function
 * when next executed following the call to GoToNextScreen().
 *
 * Entry arg:  screenID = ID number of next screen required.
 */
void GoToNextScreen(uint8 screenID) 
{
  nextScreen = screenID;
  screenSwitchReq = TRUE;
}

/*
 * Called at periodic intervals of 50 milliseconds from the main loop, the
 * User Interface task allows the user to view and adjust various operational
 * parameters using the Data Entry Pot, push-buttons and IIC OLED display.
 */
void UserInterfaceTask(void) 
{
  if (screenSwitchReq)  // Screen switch requested
  {
    screenSwitchReq = FALSE;
    isNewScreen = TRUE;              // Signal to render a new screen
    previousScreen = currentScreen;  // Make the switch...
    currentScreen = nextScreen;      // next screen => current
    if (nextScreen != previousScreen) Disp_ClearScreen();
    PotMoved();  // clear 'pot moved' flag
  }

  switch (currentScreen) 
  {
    case STARTUP:            UserState_StartupScreen();      break;
    case CALIBRATE_CV1:      UserState_Calibrate_CV1();      break;
    case CONFIRM_DEFAULT:    UserState_ConfirmDefault();     break;
    case HOME_SCREEN:        UserState_HomeScreen();         break;
    case SELECT_PRESET:      UserState_SelectPreset();       break;
    case SETUP_MENU:         UserState_SetupMenu();          break;
    case SHOW_MESSAGE:       UserState_ShowUserMessage();    break;
    //
    case SET_AMPLD_CTRL:     UserState_SetAmpldControl();    break;
    case SET_VIBRATO_MODE:   UserState_SetVibratoMode();     break;
    case SET_PITCH_BEND:     UserState_SetPitchBendMode();   break;
    case SET_REVERB_LEVEL:   UserState_SetReverbLevel();     break;
    case SET_CV_OPTIONS:     UserState_SetCVOptions();       break;
    case SET_MASTER_TUNE:    UserState_SetMasterTune();      break;
    //
    case SET_LFO_DEPTH:      UserState_Set_LFO_FM_Depth();   break;
    case SET_LFO_FREQ:       UserState_Set_LFO_Freq();       break;
    case SET_LFO_RAMP:       UserState_Set_LFO_RampTime();   break;  
    case SET_ENV_ATTACK:     UserState_Set_ENV_Attack();     break;
    case SET_ENV_HOLD:       UserState_Set_ENV_Hold();       break;
    case SET_ENV_DECAY:      UserState_Set_ENV_Decay();      break;
    case SET_ENV_SUSTAIN:    UserState_Set_ENV_Sustain();    break;
    case SET_ENV_RELEASE:    UserState_Set_ENV_Release();    break;
    case SET_MIXER_GAIN:     UserState_Set_MixerGain();      break;
    case SET_LIMITER_LVL:    UserState_Set_LimiterLevel();   break;
  }

  isNewScreen = FALSE;
}


//===========  A P P L I C A T I O N - S P E C I F I C   F U N C T I O N S  =============

void UserState_StartupScreen() 
{
  if (isNewScreen) 
  {
    DisplayTitleBar("Start-up");
    Disp_PosXY(116, 1);
    Disp_PutImage(config_icon_9x9, 9, 9);  // Config icon
    Disp_SetFont(MONO_8_NORM);
    Disp_PosXY(4, 16);
    if (g_CVcontrolMode) 
    {
      Disp_PutText("Sigma-6 Voice Module");
      DisplayButtonLegend(BUTT_POS_A, "Cal CV1");
      Disp_SetFont(PROP_8_NORM);
      Disp_PosXY(8, 42);
      Disp_PutText("(Wait for Home screen)");
    }
    else  // MIDI control mode...
    {
      Disp_PutText("ItsyBitsy M0 Synth");
      DisplayButtonLegend(BUTT_POS_A, "Home");
      Disp_SetFont(PROP_8_NORM);
      Disp_PosXY(4, 40);
      Disp_PutText("MIDI channel: ");
      Disp_SetFont(MONO_8_NORM);
      if (g_MidiMode == OMNI_ON_MONO) Disp_PutText("Omni");  
      else  Disp_PutDecimal(g_MidiChannel, 1);
    }
    DisplayButtonLegend(BUTT_POS_B, "Default");

    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(4, 28);
    Disp_PutText("Firmware version: ");
    Disp_SetFont(MONO_8_NORM);
    Disp_PutText(FIRMWARE_VERSION);  // defined in m0_synth_def.h
  }

  if (ButtonHit('A'))
  {
	  if (g_CVcontrolMode) GoToNextScreen(CALIBRATE_CV1);
	  else  GoToNextScreen(HOME_SCREEN);
  }
  if (ButtonHit('B')) GoToNextScreen(CONFIRM_DEFAULT);

  // Exit this screen ~5 sec. after start-up
  if (millis() > 5000) GoToNextScreen(HOME_SCREEN);
}


void UserState_ConfirmDefault()
{
  static bool  affirmed;
  static uint32 timeSinceAffirm_ms;

  if (isNewScreen) 
  {
    DisplayTitleBar("Default Config");
    Disp_Mode(SET_PIXELS);
    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(8, 16);
    Disp_PutText("Restore configuration");
    Disp_PosXY(8, 26);
    Disp_PutText("parameters to default");
    Disp_PosXY(8, 36);
    Disp_PutText("settings ?");
    DisplayButtonLegend(BUTT_POS_A, "Cancel");
    DisplayButtonLegend(BUTT_POS_B, "Affirm");
    timeSinceAffirm_ms = 0;
    affirmed = FALSE;
  }

  if (ButtonHit('A')) GoToNextScreen(HOME_SCREEN);
  if (ButtonHit('B') && !affirmed) 
  {
    DefaultConfigData();
    // *** todo: save config struct in EEPROM
    Disp_PosXY(0, 16);
    Disp_BlockClear(128, 30);  // Erase messages
    Disp_SetFont(PROP_12_NORM);
    // *** todo:  reinstate these 2 lines when EEPROM supported...    
//  Disp_PosXY(48, 26);  // mid-screen
//  Disp_PutText("Done!");
    // *** meanwhile, show this message...................
    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(0, 26);
    Disp_PutText("<!> EEPROM not supported");
    Disp_PosXY(0, 36);
    Disp_PutText("  in firmware version 1.2");
    // *** end of temporary code .........................
    DisplayButtonLegend(BUTT_POS_A, "X");
    DisplayButtonLegend(BUTT_POS_B, "X");
    affirmed = TRUE;
    timeSinceAffirm_ms = 0;
  }
  
  if (affirmed) 
  {
    if (timeSinceAffirm_ms >= 3000)  GoToNextScreen(HOME_SCREEN);  // todo: 1500 ms
    timeSinceAffirm_ms += 50;
  }
}


void UserState_HomeScreen() 
{
  static uint8 lastPresetShown;
  uint8 preset_id = g_Config.PresetLastSelected;

  if (isNewScreen) 
  {
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(0, 0);
    Disp_BlockFill(28, 25);
    Disp_Mode(CLEAR_PIXELS);
    Disp_PosXY(2, 2);
    Disp_PutImage(sigma_6_icon_24x21, 24, 21);

    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    if (!g_CVcontrolMode)
      Disp_PutImage(midi_conn_icon_9x9, 9, 9);  // MIDI DIN5 icon

    Disp_SetFont(PROP_12_BOLD);
    Disp_PosXY(32, 2);
    Disp_PutText("Sigma 6");
    Disp_SetFont(MONO_8_NORM);
    Disp_PosXY(32, 16);
    if (g_CVcontrolMode) Disp_PutText("Voice Module");
    else  Disp_PutText("ItsyBitsy Synth");

    DisplayButtonLegend(BUTT_POS_A, "PRESET");
    DisplayButtonLegend(BUTT_POS_B, "SETUP");
    lastPresetShown = 255;  // force refresh
  }

  if (ButtonHit('A')) GoToNextScreen(SELECT_PRESET);
  if (ButtonHit('B')) GoToNextScreen(SETUP_MENU);

  // Refresh Preset displayed if selection changed...
  if (lastPresetShown != g_Config.PresetLastSelected) 
  {
    Disp_PosXY(0, 36);
    Disp_BlockClear(128, 8);  // erase existing line
    Disp_SetFont(MONO_8_NORM);
    Disp_PutDecimal(g_Config.PresetLastSelected, 2);  // Preset #
    Disp_PutChar(' ');
    Disp_SetFont(PROP_8_NORM);
    Disp_PutText((char *)g_PresetPatch[preset_id].PresetName);
    lastPresetShown = g_Config.PresetLastSelected;
  }
}


void UserState_SelectPreset() 
{
  static uint32 timeSinceLastRefresh_ms;  // unit = ms
  static uint8 setting;
  static bool settingChanged;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("SELECT PRESET");
    DisplayButtonLegend(BUTT_POS_A, "Cancel");
    DisplayButtonLegend(BUTT_POS_B, "Affirm");
    setting = g_Config.PresetLastSelected;
    settingChanged = TRUE;  // signal to refresh preset shown
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 8;  // 0..32
    if (setting >= GetNumberOfPresets()) setting = GetNumberOfPresets() - 1;
    settingChanged = TRUE;
    doRefresh = TRUE;
  }

  if (ButtonHit('A')) GoToNextScreen(HOME_SCREEN);  // no change
  if (ButtonHit('B'))  // Affirm new setting
  {
    PresetSelect(setting);
    GoToNextScreen(HOME_SCREEN);
  }

  if (doRefresh)  
  {
    Disp_SetFont(MONO_16_NORM);  // Display preset number
    Disp_PosXY(48, 20);
    Disp_BlockClear(40, 16);  // clear existing data
    Disp_PutDecimal(setting, 2);
    timeSinceLastRefresh_ms = 0;  // reset timer
  }

  if (settingChanged && timeSinceLastRefresh_ms >= 350) 
  {
    Disp_SetFont(PROP_8_NORM);  // Display preset name
    Disp_PosXY(8, 38);
    Disp_BlockClear(120, 8);  // erase existing line
    Disp_PutText((char *)g_PresetPatch[setting].PresetName);
    settingChanged = FALSE;  // prevent repeat refresh
    timeSinceLastRefresh_ms = 0;
  }

  timeSinceLastRefresh_ms += 50;  // Call period is 50ms
}


void UserState_SetupMenu() 
{
  static const char *parameterName[] = { "Ampld Control", "Vibrato", "Pitch Bend", "Reverb",
    "CV Options", "Master Tune", "LFO Depth", "LFO Freq", "LFO Ramp", "ENV Attack", "ENV Hold",
    "ENV Decay", "ENV Sustain", "ENV Release", "Mixer Gain", "Limiter"
  };
  static uint8 setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("   SETUP MENU");
    Disp_PosXY(116, 1);
    Disp_PutImage(config_icon_9x9, 9, 9);  // Config icon
    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(4, 18);
    Disp_PutText("Parameter to adjust:");
    DisplayButtonLegend(BUTT_POS_A, "Home");
    DisplayButtonLegend(BUTT_POS_B, "Select");
    strcpy(messageLine1, "Sorry, this function is");
    strcpy(messageLine2, "not yet implemented !");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 16;  // 0..16
    if (g_CVcontrolMode && setting == 2)  setting = 3;   // bypass Pitch-bend setup in CV mode
    if (!g_CVcontrolMode && setting == 4)  setting = 5;  // bypass CV Options setup in MIDI mode
    doRefresh = TRUE;
  }

  if (ButtonHit('A')) GoToNextScreen(HOME_SCREEN);
  if (ButtonHit('B'))  // Affirm selection...
  {
    switch (setting)
    {
      case  0:  GoToNextScreen(SET_AMPLD_CTRL);    break;
      case  1:  GoToNextScreen(SET_VIBRATO_MODE);  break;
      case  2:  GoToNextScreen(SET_PITCH_BEND);    break;
      case  3:  GoToNextScreen(SET_REVERB_LEVEL);  break;
      case  4:  GoToNextScreen(SET_CV_OPTIONS);    break;
      case  5:  GoToNextScreen(SET_MASTER_TUNE);   break;
      case  6:  GoToNextScreen(SET_LFO_DEPTH);     break;
      case  7:  GoToNextScreen(SET_LFO_FREQ);      break;
      case  8:  GoToNextScreen(SET_LFO_RAMP);      break;
      case  9:  GoToNextScreen(SET_ENV_ATTACK);    break;
      case 10:  GoToNextScreen(SET_ENV_HOLD);      break; 
      case 11:  GoToNextScreen(SET_ENV_DECAY);     break; 
      case 12:  GoToNextScreen(SET_ENV_SUSTAIN);   break; 
      case 13:  GoToNextScreen(SET_ENV_RELEASE);   break; 
      case 14:  GoToNextScreen(SET_MIXER_GAIN);    break;
      case 15:  GoToNextScreen(SET_LIMITER_LVL);   break;
      default:  GoToNextScreen(SHOW_MESSAGE);      break;
    } // end switch
  }

  if (doRefresh)  
  {
    Disp_SetFont(PROP_12_NORM);  // Display parameter name
    Disp_PosXY(16, 32);
    Disp_BlockClear(96, 12);  // clear existing data
    Disp_PutText(parameterName[setting]);
  }
}


void UserState_ShowUserMessage()
{
  static uint32 timeElapsed_ms;

  if (isNewScreen) 
  {
    Disp_Mode(SET_PIXELS);
    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(4, 16);
    Disp_PutText(messageLine1);
    Disp_PosXY(4, 26);
    Disp_PutText(messageLine2);
    DisplayButtonLegend(BUTT_POS_A, "Home");
    DisplayButtonLegend(BUTT_POS_B, "Back");  // to Setup menu
    timeElapsed_ms = 0;
  }

  if (ButtonHit('A')) GoToNextScreen(HOME_SCREEN);
  if (ButtonHit('B')) GoToNextScreen(SETUP_MENU);

  if (timeElapsed_ms >= 3000) GoToNextScreen(HOME_SCREEN);
  timeElapsed_ms += 50;
}


void UserState_SetAmpldControl() 
{
  static uint8 setting;
  static bool settingChanged;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("Ampld Control");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(config_icon_9x9, 9, 9);  // Config icon
    DisplayButtonLegend(BUTT_POS_A, "Cancel");
    DisplayButtonLegend(BUTT_POS_B, "Affirm");
    setting = g_Config.AudioAmpldCtrlMode;
    settingChanged = FALSE;
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = (PotPosition() >> 5) & 3;  // 0..3 repeating
    settingChanged = TRUE;
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);  // Exit
  if (ButtonHit('B'))  // Affirm
  {
    if (settingChanged) 
    {
      g_Config.AudioAmpldCtrlMode = setting;
      // todo: store setting in eeprom
    }
    GoToNextScreen(SET_VIBRATO_MODE);
  }

  if (doRefresh) 
  {
    Disp_SetFont(PROP_12_NORM);
    Disp_PosXY(24, 20);
    Disp_BlockClear(104, 12);  // erase existing text
    if (setting == AUDIO_CTRL_CONST) Disp_PutText("Fixed (100%)");
    else if (setting == AUDIO_CTRL_ENV1_VELO) Disp_PutText("ENV1 * Velo");
    else if (setting == AUDIO_CTRL_EXPRESS) Disp_PutText("Expression");
    else Disp_PutText("from Preset");

    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(24, 36);
    Disp_BlockClear(104, 8);  // erase existing text
    if (setting != AUDIO_CTRL_BY_PATCH) Disp_PutText("(Override patch)");
    else  Disp_PutText("(Use patch param.)");
  }
}


void UserState_SetVibratoMode() 
{
  static uint8 setting;
  static bool settingChanged;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("Vibrato Mode");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(config_icon_9x9, 9, 9);  // Config icon
    DisplayButtonLegend(BUTT_POS_A, "Cancel");
    DisplayButtonLegend(BUTT_POS_B, "Affirm");
    setting = g_Config.VibratoCtrlMode;
    settingChanged = FALSE;
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = (PotPosition() >> 5) & 3;  // 0..3 repeating
    settingChanged = TRUE;
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);  // Exit
  if (ButtonHit('B'))  // Affirm
  {
    if (settingChanged) 
    {
      g_Config.VibratoCtrlMode = setting;
      // todo: store setting in eeprom
    }
    if (g_CVcontrolMode) GoToNextScreen(SET_REVERB_LEVEL);  // skip Pitch-bend
    else  GoToNextScreen(SET_PITCH_BEND);
  }

  if (doRefresh) 
  {
    Disp_SetFont(PROP_12_NORM);
    Disp_PosXY(24, 26);
    Disp_BlockClear(104, 12);  // erase existing text line
    if (setting == VIBRATO_BY_CV_AUXIN) Disp_PutText("CV4 (AUX.IN)");
    else if (setting == VIBRATO_BY_MODN_CC) Disp_PutText("MIDI message");
    else if (setting == VIBRATO_AUTOMATIC) Disp_PutText("Automatic");
    else Disp_PutText("Disabled");
  }
}


void UserState_SetPitchBendMode()
{
  static uint8 setting;
  static bool settingChanged;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("Pitch Bend");
    Disp_PosXY(116, 1);
    Disp_PutImage(config_icon_9x9, 9, 9);  // Config icon
    DisplayButtonLegend(BUTT_POS_A, "Cancel");
    DisplayButtonLegend(BUTT_POS_B, "Affirm");
    setting = g_Config.PitchBendMode;
    settingChanged = FALSE;
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = (PotPosition() >> 5) & 3;  // 0..3 repeating
    settingChanged = TRUE;
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);  // Exit
  if (ButtonHit('B'))  // Affirm
  {
    if (settingChanged) 
    {
      g_Config.PitchBendMode = setting;
      // todo: store setting in eeprom
    }
    GoToNextScreen(SET_REVERB_LEVEL);
  }

  if (doRefresh) 
  {
    Disp_SetFont(PROP_12_NORM);
    Disp_PosXY(24, 26);
    Disp_BlockClear(104, 12);  // erase existing text
    if (setting == PITCH_BEND_BY_MIDI_MSG)  Disp_PutText("MIDI message");
    else if (setting == PITCH_BEND_BY_CV1_INPUT)  Disp_PutText("CV1 input");
    else  Disp_PutText("Disabled");
  }
}


void UserState_SetReverbLevel() 
{
  static uint8 setting;
  static bool settingChanged;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("Reverb Level");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(config_icon_9x9, 9, 9);  // Config icon
    DisplayButtonLegend(BUTT_POS_A, "Cancel");
    DisplayButtonLegend(BUTT_POS_B, "Affirm");
    setting = g_Config.ReverbMix_pc;
    settingChanged = FALSE;
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = ((int)PotPosition() * 100) / 255;  // 0..100
    setting = (setting / 5) * 5;  // quantize, step size = 5
    g_Patch.LFO_FM_Depth = setting;
    settingChanged = TRUE;
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);  // Exit
  if (ButtonHit('B'))  // Affirm
  {
    if (settingChanged) 
    {
      g_Config.ReverbMix_pc = setting;
      // todo: store setting in eeprom
    }
    GoToNextScreen(SET_LFO_DEPTH);  // Skip CV options and Master Tune
  }

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(48, 24);
    Disp_BlockClear(64, 16);  // clear existing data
    Disp_PutDecimal(setting, 2);
    Disp_SetFont(PROP_12_NORM);
    Disp_PutText(" %");
  }
}


void UserState_SetCVOptions()
{
  static uint8 setting;
  static bool settingChanged;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("CV Options");
    Disp_PosXY(116, 1);
    Disp_PutImage(config_icon_9x9, 9, 9);  // Config icon
    Disp_SetFont(MONO_8_NORM);
    Disp_PosXY(4, 16);
    Disp_PutText("Pitch Quantize....");
    DisplayButtonLegend(BUTT_POS_A, "Cancel");
    DisplayButtonLegend(BUTT_POS_B, "Affirm");
    setting = g_Config.Pitch_CV_Quantize;
    settingChanged = FALSE;
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = (PotPosition() >> 4) & 7;  // 0..7 repeating
    settingChanged = TRUE;
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);  // Exit
  if (ButtonHit('B'))  // Affirm
  {
    if (settingChanged) 
    {
      g_Config.Pitch_CV_Quantize = setting & 1;
      // todo: store setting in eeprom
    }
    GoToNextScreen(SET_MASTER_TUNE);
  }

  if (doRefresh) 
  {
    Disp_SetFont(MONO_8_NORM);
    Disp_PosXY(104, 16);
    Disp_BlockClear(24, 8);  // erase existing text
    if (setting & 1)  Disp_PutText("ON");
    else  Disp_PutText("OFF");
  }
}


void UserState_SetMasterTune()
{
  static short setting;  // signed
  static bool settingChanged;
  short  absValue;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("Master Tune");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(config_icon_9x9, 9, 9);  // Config icon
    Disp_SetFont(MONO_8_NORM);
    Disp_PosXY(96, 31);
    Disp_PutText("cent");  
    DisplayButtonLegend(BUTT_POS_A, "Cancel");
    DisplayButtonLegend(BUTT_POS_B, "Affirm");
    setting = g_Config.MasterTuneOffset;
    settingChanged = FALSE;
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = ((short) PotPosition() * 100) / 255 - 50;  // -50 to +50
    g_Config.MasterTuneOffset = setting;
    settingChanged = TRUE;
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);  // Exit
  if (ButtonHit('B'))  // Affirm
  {
    if (settingChanged) 
    {
      g_Config.MasterTuneOffset = setting;
      // todo: store setting in eeprom
    }
    GoToNextScreen(SET_LFO_DEPTH);
  }

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(40, 24);
    Disp_BlockClear(36, 16);  // clear existing data
    absValue = setting;
    if (setting < 0)  { absValue = 0 - setting;  Disp_PutChar('-'); }
    else if (setting > 0)  Disp_PutChar('+');
    else  Disp_PutChar(' ');  // setting == 0: put space
    Disp_PutDecimal(absValue, 2);  // show leading zero
  }
}


void UserState_Set_LFO_FM_Depth()
{
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("LFO FM Depth");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = ((int)PotPosition() * 100) / 255;  // 0..100
    setting = (setting / 5) * 5;    // quantize, step size = 5
    g_Patch.LFO_FM_Depth = setting;
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_LFO_FREQ);

  if (doRefresh) 
  {
    if (g_Config.VibratoCtrlMode == VIBRATO_BY_CV_AUXIN)
    {
      Disp_SetFont(PROP_12_NORM);
      Disp_PosXY(48, 24);
      Disp_PutText("N/A");
      Disp_SetFont(PROP_8_NORM);
      Disp_PosXY(8, 38);
      Disp_PutText("[LFO depth set by CV4]");
    }
    else 
    {
      Disp_SetFont(MONO_16_NORM);
      Disp_PosXY(48, 24);
      Disp_BlockClear(64, 16);  // clear existing data
      Disp_PutDecimal(g_Patch.LFO_FM_Depth, 2);
      Disp_SetFont(PROP_12_NORM);
      Disp_PutText(" %");
    }
  }
}


void UserState_Set_LFO_Freq() 
{
  static uint8  LFOfreqStep[] = 
    { 5, 7, 10, 15, 20, 25, 30, 40, 50, 60, 70, 80, 100, 150, 200, 250 };  // 16 steps
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("LFO Frequency");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 16;  // 0..15
    g_Patch.LFO_Freq_x10 = LFOfreqStep[setting];
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_LFO_RAMP);

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(40, 24);
    Disp_BlockClear(64, 16);  // clear existing data
    if (g_Patch.LFO_Freq_x10 >= 10 && g_Patch.LFO_Freq_x10 < 100) Disp_PutChar(' ');
    Disp_PutDecimal(g_Patch.LFO_Freq_x10 / 10, 1);  // integer part
    if (g_Patch.LFO_Freq_x10 < 10)
    {
      Disp_PutChar('.');
      Disp_PutDecimal(g_Patch.LFO_Freq_x10 % 10, 1);  // fractional part
    }
    Disp_SetFont(PROP_12_NORM);
    Disp_PosXY(Disp_GetX(), 28);
    Disp_PutText(" Hz");
  }
}


void UserState_Set_LFO_RampTime() 
{
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("LFO Ramp Time");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 16;  // 0..15
    setting = timeValueQuantized[setting];  // one of 16 values
    g_Patch.LFO_RampTime = (uint16) setting;  // range 5..5000
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_ENV_ATTACK);

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(40, 24);
    Disp_BlockClear(80, 16);  // clear existing data
    Disp_PutDecimal(g_Patch.LFO_RampTime, 1);  // up to 4 digits
    Disp_SetFont(PROP_12_NORM);
    Disp_PosXY(Disp_GetX(), 28);
    Disp_PutText(" ms");
  }
}


void UserState_Set_ENV_Attack()
{
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("ENV Attack");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 16;  // 0..15
    setting = timeValueQuantized[setting];  // one of 16 values
    if (setting == 0)  setting = 5;  // minimum
    g_Patch.EnvAttackTime = (uint16) setting;  // range 5..5000
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_ENV_HOLD);

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(40, 24);
    Disp_BlockClear(80, 16);  // clear existing data
    Disp_PutDecimal(g_Patch.EnvAttackTime, 1);  // up to 4 digits
    Disp_SetFont(PROP_12_NORM);
    Disp_PosXY(Disp_GetX(), 28);
    Disp_PutText(" ms");
  }
}


void  UserState_Set_ENV_Hold()
{
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("ENV Peak-Hold");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(4, 38);
    Disp_PutText("[0 = bypass Peak & Decay]");
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 16;  // 0..15
    setting = timeValueQuantized[setting];  // one of 16 values
    g_Patch.EnvHoldTime = (uint16) setting;  // range 0..5000
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_ENV_DECAY);

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(40, 20);
    Disp_BlockClear(80, 16);  // clear existing data
    Disp_PutDecimal(g_Patch.EnvHoldTime, 1);  // up to 4 digits
    Disp_SetFont(PROP_12_NORM);
    Disp_PosXY(Disp_GetX(), 24);
    if (g_Patch.EnvHoldTime != 0) Disp_PutText(" ms");
  }
}


void  UserState_Set_ENV_Decay()
{
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("ENV Decay");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(8, 38);
    if (g_Patch.EnvHoldTime != 0) Disp_PutText("Set Hold = 0 to bypass");
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 16;  // 0..15
    setting = timeValueQuantized[setting];  // one of 16 values
    if (setting == 0)  setting = 5;  // minimum
    g_Patch.EnvDecayTime = (uint16) setting;  // range 5..5000
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_ENV_SUSTAIN);

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(40, 20);
    Disp_BlockClear(80, 16);  // clear existing data
    Disp_PosXY(8, 38);
    Disp_BlockClear(120, 8);  // clear existing message
    if (g_Patch.EnvHoldTime != 0)
    {
      Disp_PutDecimal(g_Patch.EnvDecayTime, 1);
      Disp_SetFont(PROP_12_NORM);
      Disp_PosXY(Disp_GetX(), 24);
      Disp_PutText(" ms");
    }
    else  // Decay segment bypassed
    {
      Disp_PutText(" X ");
      Disp_SetFont(PROP_8_NORM);
      Disp_PosXY(32, 38);
      Disp_PutText("(bypassed)");
    }
  }
}


void  UserState_Set_ENV_Sustain()
{
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("ENV Sustain");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(24, 38);
    Disp_PutText("[logarithmic scale]");
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 16;  // 0..15
    setting = percentLogScale[setting];  // 0..100%
    g_Patch.EnvSustainLevel = setting;
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_ENV_RELEASE);

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(48, 20);
    Disp_BlockClear(64, 16);  // clear existing data
    Disp_PutDecimal(g_Patch.EnvSustainLevel, 2);
    Disp_SetFont(PROP_12_NORM);
    Disp_PutText(" %");
  }
}


void UserState_Set_ENV_Release()
{
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("ENV Release");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 16;  // 0..15
    setting = timeValueQuantized[setting];  // one of 16 values
    if (setting == 0)  setting = 5;  // minimum
    g_Patch.EnvReleaseTime = (uint16) setting;  // range 5..5000
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_MIXER_GAIN);

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(40, 24);
    Disp_BlockClear(80, 16);  // clear existing data
    Disp_PutDecimal(g_Patch.EnvReleaseTime, 1);  // up to 4 digits
    Disp_SetFont(PROP_12_NORM);
    Disp_PosXY(Disp_GetX(), 28);
    Disp_PutText(" ms");
  }
}


void  UserState_Set_MixerGain()
{
  static uint8 optMixerGain_x10[] = { 2, 3, 5, 7, 10, 15, 20, 25, 33, 50 };
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("Mixer Gain");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = PotPosition() / 25;  // 0..10
    if (setting > 9)  setting = 9;  // 0..9 (index)
    g_Patch.MixerOutGain_x10 = (uint16) optMixerGain_x10[setting];
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_LIMITER_LVL);

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(48, 24);
    Disp_BlockClear(64, 16);  // clear existing data
    Disp_PutDecimal(g_Patch.MixerOutGain_x10 / 10, 1);  // integer part
    Disp_PutChar('.');
    Disp_PutDecimal(g_Patch.MixerOutGain_x10 % 10, 1);  // fractional part
  }
}


void  UserState_Set_LimiterLevel()
{
  int  setting;
  bool doRefresh = FALSE;

  if (isNewScreen) 
  {
    DisplayTitleBar("Limiter Level");
    Disp_Mode(SET_PIXELS);
    Disp_PosXY(116, 1);
    Disp_PutImage(patch_icon_9x9, 9, 9);  // Patch icon
    DisplayButtonLegend(BUTT_POS_A, "Exit");
    DisplayButtonLegend(BUTT_POS_B, "Next");
    doRefresh = TRUE;
  }

  if (PotMoved()) 
  {
    setting = ((int)PotPosition() * 100) / 255;  // 0..100
    setting = (setting / 5) * 5;  // quantize, step size = 5
    if (setting < 20) setting = 0;  // floor = 20%
    if (setting > 95) setting = 95;  // ceiling = 95%
    g_Patch.LimiterLevelPc = setting;
    doRefresh = TRUE;
  }

  if (ButtonHit('A'))  GoToNextScreen(SETUP_MENU);
  if (ButtonHit('B'))  GoToNextScreen(SET_LFO_DEPTH);  // back to 1st patch param

  if (doRefresh) 
  {
    Disp_SetFont(MONO_16_NORM);
    Disp_PosXY(48, 24);
    Disp_BlockClear(64, 16);  // clear existing data
    if (g_Patch.LimiterLevelPc == 0) Disp_PutText("OFF");
    else
    {
      Disp_PutDecimal(g_Patch.LimiterLevelPc, 2);
      Disp_SetFont(PROP_12_NORM);
      Disp_PutText(" %");
    }
  }
}


void  UserState_Calibrate_CV1() 
{
  static int  callsSinceLastRefresh;
  static int  aveReading_x256;  // ADC reading average, fixed-point (8-bit frac.)
  int  calibReading_mV, thisReading_mV;

  // Read CV1 input and apply first-order IIR filter (rolling average), K = 0.125
  thisReading_mV = ((int) analogRead(A1) * 5100) / 4095;
  aveReading_x256 -= aveReading_x256 / 8;
  aveReading_x256 += (thisReading_mV << 8) / 8;

  if (isNewScreen) 
  {
    DisplayTitleBar("Calibrate CV1");
    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(4, 26);  Disp_PutText("ADC count");
    Disp_PosXY(72, 26);  Disp_PutText("CV1 input");
    DisplayButtonLegend(BUTT_POS_A, "Done");
    DisplayButtonLegend(BUTT_POS_B, "Exit");
    callsSinceLastRefresh = 0;
  }

  if (ButtonHit('A')) GoToNextScreen(HOME_SCREEN);
  if (ButtonHit('B')) GoToNextScreen(HOME_SCREEN);
  
  if (++callsSinceLastRefresh >= 10)  // refresh every 0.5 sec
  {
    Disp_SetFont(PROP_12_NORM);
    Disp_PosXY(4, 36);
    Disp_BlockClear(120, 12);  // clear existing data
    Disp_PutDecimal((uint16) analogRead(A1), 4);  // ADC count
	  Disp_PosXY(72, 36);
    Disp_PutDecimal((uint16) (aveReading_x256 >> 8), 4);  // mV
    Disp_SetFont(PROP_8_NORM);
    Disp_PosXY(Disp_GetX(), 39);
    Disp_PutText(" mV");
    callsSinceLastRefresh = 0;
  }
}
