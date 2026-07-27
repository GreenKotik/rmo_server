#ifndef DEVICEIDS_H
#define DEVICEIDS_H

// ============================================================================
// ✅ УНИКАЛЬНЫЕ КОДЫ ТИПОВ ОБОРУДОВАНИЯ
// Блочная схема: каждая категория = свой диапазон 100 номеров
// Формат: <ПРЕФИКС КАТЕГОРИИ>_<НАЗВАНИЕ> = <номер в блоке>
//
// Блоки:
//   ANT_      (101-199)  — Контроллеры антенн
//   ENC_      (201-299)  — Энкодеры
//   POS_      (301-399)  — Позиционеры
//   PILOT_    (401-499)  — Приёмники пилот-сигнала
//   TX_       (501-599)  — Передатчики
//   TX_SW_    (601-699)  — Свичи передатчиков
//   UPC_      (701-799)  — UP-конверторы
//   UPC_SW_   (801-899)  — Свичи UP-конверторов
//   TEMP_SNS_ (901-999)  — Датчики температуры
//   TEMP_RX_  (1001-1099)— Приёмники температуры
//   DEHYD_    (1101-1199)— Дегидраторы
//   ANLZ_     (1201-1299)— Анализаторы
//   UPS_      (1301-1399)— ИБП и питание
//   CTRL_     (1401-1499)— Контрольно-силовое оборудование
//   MNEMO_    (1501-1599)— Мнемосхемы
//   MOD_      (1601-1699)— Модуляторы (зарезервировано)
//
// НЕ МЕНЯТЬ существующие значения! Только добавлять новые в свободные слоты.
// ============================================================================

// ==================== СЛУЖЕБНЫЕ КОДЫ ====================
#define ID_UNKNOWN                      0     // Устройство не определено
#define ID_NONE                         -1    // Устройство отсутствует

// ==================== КОНТРОЛЛЕРЫ АНТЕНН (101-199) ====================
#define ID_ANT_ELECTROSPACE_93C_23      101
#define ID_ANT_ELECTROSPACE_93C_23F     102
#define ID_ANT_VERTEX_7200_ACS          103
#define ID_ANT_DELTA_SU5_1              104
#define ID_ANT_DELTA_SU12B              105
#define ID_ANT_DELTA_SU4_8              106
#define ID_ANT_VERTEX_7134_ACS          107
#define ID_ANT_ADAM_5510E               108
#define ID_ANT_SIMATIC_IPC427C          109

// ==================== ЭНКОДЕРЫ (201-299) ====================
#define ID_ENC_FRABA_OCD_DPB1B          201

// ==================== ПОЗИЦИОНЕРЫ (301-399) ====================
#define ID_POS_MICROMASTER_440          301
#define ID_POS_ICPDAS_I_87068           302

// ==================== ПРИЁМНИКИ ПИЛОТ-СИГНАЛА (401-499) ====================
#define ID_PILOT_TUNER_TC_70_04         401
#define ID_PILOT_ADAM_4017              402
#define ID_PILOT_VERTEX_TRL_2S          403
#define ID_PILOT_VERTEX_CG_1220         404
#define ID_PILOT_ASC300LW               405
#define ID_PILOT_BEACON_3430            406
#define ID_PILOT_PEAK_RTR50             407
#define ID_PILOT_ICPCON_TM_AD8          408
#define ID_PILOT_PROBECOM_BR3500L       409
#define ID_PILOT_GD_DTR                 410
#define ID_PILOT_TR_PSN                 411

// ==================== ПЕРЕДАТЧИКИ (501-599) ====================
#define ID_TX_ADVANTECH_ARMAC_250       501
#define ID_TX_ADVANTECH_ARMAG_CRL400    502
#define ID_TX_CPI_VZU_6997AC            503
#define ID_TX_MAXTECH_PDC6200N          504
#define ID_TX_MCL_MT3000                505
#define ID_TX_MCL_MT5000                506
#define ID_TX_PARADISE_HPAC2250BC       507
#define ID_TX_PARADISE_HPAC2600BR       508
#define ID_TX_VARIAN_VZC_6965F7         509
#define ID_TX_VARIAN_VZU_6995F6         510
#define ID_TX_VARIAN_VZU_6994A3         511
#define ID_TX_XICOM_XTRD_400C           512
#define ID_TX_XICOM_XTRD_750C           513
#define ID_TX_XICOM_XTRD_750K           514
#define ID_TX_CPI_TL04CO                515
#define ID_TX_CPI_TL05KO                516
#define ID_TX_CPI_VZU_6997AF            517
#define ID_TX_PEAK_UPC7000              518
#define ID_TX_RADIS_UM5765_600P         519
#define ID_TX_PROBECOM_40W_KA_BUC       520

// ==================== СВИЧИ ПЕРЕДАТЧИКОВ (601-699) ====================
#define ID_TX_SW_INTERNAL               601
#define ID_TX_SW_CPI_NCB_1100X          602
#define ID_TX_SW_VARIAN_VJC_6767S       603
#define ID_TX_SW_VARIAN_VZC_6965P6      604
#define ID_TX_SW_XICOM_XTC_100D         605
#define ID_TX_SW_XICOM_XTC_114D         606
#define ID_TX_SW_MCL_COMBINER           607
#define ID_TX_SW_CPI_MODEL_0103233001_1k2 608
#define ID_TX_SW_CPI_MODEL_0103233001_1k1 609

// ==================== UP-КОНВЕРТОРЫ (701-799) ====================
#define ID_UPC_COMTECH_UT_4500          701
#define ID_UPC_WSI_UT_4500              702
#define ID_UPC_MITEQ_U_9900             703
#define ID_UPC_PEAK_IBU655              704

// ==================== СВИЧИ UP-КОНВЕРТОРОВ (801-899) ====================
#define ID_UPC_SW_MITEQ_NSU             801
#define ID_UPC_SW_PEAK_RCU100           802
#define ID_UPC_SW_ADVANTECH_ARUD_LCXR   803
#define ID_UPC_SW_IRT_IFC_L1C210R_RM00  804

// ==================== ДАТЧИКИ ТЕМПЕРАТУРЫ (901-999) ====================
#define ID_TEMP_SNS_TE_MONITOR_V6       901
#define ID_TEMP_SNS_BOREAS_3            902

// ==================== ПРИЁМНИКИ ТЕМПЕРАТУРЫ (1001-1099) ====================
#define ID_TEMP_RX_ICPCON_I_87017       1001
#define ID_TEMP_RX_ICPCON_ET_7017       1002
#define ID_TEMP_RX_ADVANTECH_ADAM_4017  1003

// ==================== ДЕГИДРАТОРЫ (1101-1199) ====================
#define ID_DEHYD_EDH_4                  1101
#define ID_DEHYD_ADH_NETCOM             1102
#define ID_DEHYD_HR150                  1103
#define ID_DEHYD_ADBKP_2                1104

// ==================== АНАЛИЗАТОРЫ (1201-1299) ====================
#define ID_ANLZ_AGILENT_E4418B          1201

// ==================== ИБП И ПИТАНИЕ (1301-1399) ====================
#define ID_UPS_IBP_EATON_93XX           1301

// ==================== КОНТРОЛЬНО-СИЛОВОЕ ОБОРУДОВАНИЕ (1401-1499) ====================
#define ID_CTRL_ICPCON_ET_7017_IN       1401
#define ID_CTRL_TET_P6_JUN              1402
#define ID_CTRL_TET_P6_AVR              1403
#define ID_CTRL_OMIX_P99_M_3            1404
#define ID_CTRL_WB_MAP6S                1405
#define ID_CTRL_WB_MAP12E               1406
#define ID_CTRL_PISO_MN200              1407
#define ID_CTRL_ICPCON_TET_P6           1408
#define ID_CTRL_OVEN_MB110_8DF          1409
#define ID_CTRL_OVEN_MB110_8AC          1410
#define ID_CTRL_OVEN_MB110_16DN         1411
#define ID_CTRL_OVEN_MB110_32DN         1412
#define ID_CTRL_OVEN_ME110_3M           1413

// ==================== МНЕМОСХЕМЫ (1501-1599) ====================
#define ID_MNEMO_NEXT                   1501
#define ID_MNEMO_IBP                    1502
#define ID_MNEMO_JUNIPER                1503
#define ID_MNEMO_UPS                    1504
#define ID_MNEMO_AVR                    1505
#define ID_MNEMO_ENERGO                 1506
#define ID_MNEMO_SERVERNAYA_KROSSOVAYA  1507
#define ID_MNEMO_MARS_1                 1508

// ==================== МОДУЛЯТОРЫ (1601-1699) — ЗАРЕЗЕРВИРОВАНО ====================
// В будущем сюда добавим конкретные модели модуляторов
// Пример:
// #define ID_MOD_XXX                    1601
// #define ID_MOD_YYY                    1602

// ============================================================================
// ✅ ВСПОМОГАТЕЛЬНЫЕ МАКРОСЫ
// ============================================================================

// Извлечение категории из ID (например, 501 → 500, 1001 → 1000)
#define DEVICE_CATEGORY(id)             ((id) / 100 * 100)

// Извлечение номера устройства внутри категории (например, 503 → 3)
#define DEVICE_NUMBER_IN_CATEGORY(id)   ((id) % 100)

// Проверка категории
#define IS_ANTENNA_CONTROLLER(id)       ((id) >= 101 && (id) <= 199)
#define IS_ENCODER(id)                  ((id) >= 201 && (id) <= 299)
#define IS_POSITIONER(id)               ((id) >= 301 && (id) <= 399)
#define IS_PILOT_RECEIVER(id)           ((id) >= 401 && (id) <= 499)
#define IS_TRANSMITTER(id)              ((id) >= 501 && (id) <= 599)
#define IS_TX_SWITCH(id)                ((id) >= 601 && (id) <= 699)
#define IS_UP_CONVERTER(id)             ((id) >= 701 && (id) <= 799)
#define IS_UPC_SWITCH(id)               ((id) >= 801 && (id) <= 899)
#define IS_TEMP_SENSOR(id)              ((id) >= 901 && (id) <= 999)
#define IS_TEMP_RECEIVER(id)            ((id) >= 1001 && (id) <= 1099)
#define IS_DEHYDRATOR(id)               ((id) >= 1101 && (id) <= 1199)
#define IS_ANALYZER(id)                 ((id) >= 1201 && (id) <= 1299)
#define IS_UPS(id)                      ((id) >= 1301 && (id) <= 1399)
#define IS_CONTROL_POWER(id)            ((id) >= 1401 && (id) <= 1499)
#define IS_MNEMOSCHEMA(id)              ((id) >= 1501 && (id) <= 1599)
#define IS_MODULATOR(id)                ((id) >= 1601 && (id) <= 1699)

#endif // DEVICEIDS_H
