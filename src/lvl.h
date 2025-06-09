#ifndef LVL_H
#define LVL_H

const int allowedComponentsLevel0[] = {LINE_STRAIGHT, LINE_CORNER, LINE_T_JUNCTION};
const int allowedComponentsLevel1[] = {LINE_STRAIGHT, LINE_CORNER, LINE_T_JUNCTION, LED_STRAIGHT, LED_CORNER_R, LED_CORNER_L, RESISTOR_STRAIGHT, RESISTOR_CORNER}; 
const int allowedComponentsLevel2[] = {LINE_STRAIGHT, LINE_CORNER, LINE_T_JUNCTION, LED_STRAIGHT, LED_CORNER_R, LED_CORNER_L, RESISTOR_STRAIGHT, RESISTOR_CORNER, SW_STRAIGHT, SW_CORNER}; 
const int allowedComponentsLevel3[] = {LINE_STRAIGHT, LINE_CORNER, LINE_T_JUNCTION, LED_STRAIGHT, LED_CORNER_R, LED_CORNER_L, RESISTOR_STRAIGHT, RESISTOR_CORNER, PUSH_SW_STRAIGHT, PUSH_SW_CORNER}; 
const int allowedComponentsLevel4[] = {LINE_STRAIGHT, LINE_CORNER, LINE_T_JUNCTION, LED_STRAIGHT, LED_CORNER_R, LED_CORNER_L, RESISTOR_STRAIGHT, RESISTOR_CORNER}; 
const int allowedComponentsLevel5[] = {LINE_STRAIGHT, LINE_CORNER, LINE_T_JUNCTION, LED_STRAIGHT, LED_CORNER_R, LED_CORNER_L, RESISTOR_STRAIGHT, RESISTOR_CORNER, SW_STRAIGHT, SW_CORNER, PUSH_SW_STRAIGHT, PUSH_SW_CORNER, PHOTODIODE};

const int *allowedComponents[] = {
    allowedComponentsLevel0,
    allowedComponentsLevel1,
    allowedComponentsLevel2,
    allowedComponentsLevel3,
    allowedComponentsLevel4,
    allowedComponentsLevel5};

const int allowedComponentsCount[] = {
    sizeof(allowedComponentsLevel0) / sizeof(allowedComponentsLevel0[0]),
    sizeof(allowedComponentsLevel1) / sizeof(allowedComponentsLevel1[0]),
    sizeof(allowedComponentsLevel2) / sizeof(allowedComponentsLevel2[0]),
    sizeof(allowedComponentsLevel3) / sizeof(allowedComponentsLevel3[0]),
    sizeof(allowedComponentsLevel4) / sizeof(allowedComponentsLevel4[0]),
    sizeof(allowedComponentsLevel5) / sizeof(allowedComponentsLevel5[0])};

const uint8_t connectionMasks[][6] = {
    {1, 1, 1, 0, 0, 0},
    {0, 0, 0, 1, 1, 1},
    {1, 0, 0, 1, 1, 1},
    {0, 1, 0, 1, 1, 1},
    {0, 0, 1, 1, 1, 1},
    {1, 1, 1, 1, 0, 0},
    {1, 1, 1, 0, 1, 0},
    {1, 1, 1, 0, 0, 1},
    {0, 1, 1, 1, 1, 1},
    {1, 0, 1, 1, 1, 1},
    {1, 1, 0, 1, 1, 1},
    {1, 1, 1, 0, 1, 1},
    {1, 1, 1, 1, 0, 1},
    {1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1}};

const int connectionMasksCount = sizeof(connectionMasks) / sizeof(connectionMasks[0]);

#endif