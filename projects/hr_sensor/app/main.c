/**
  *********************************************************************************
  * SPDX-License-Identifier: Apache-2.0
  *
  * @file    main.c
  * @brief   main
  * @version V1.0
  * @author  yuzrain
  *
  *********************************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "nordic_common.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "board.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "ble_appearance_id.h"
#include "hr_svr.h"
#include "gap_svr.h"

/* Hold the address type */
static uint8_t ble_addr_type;

/* Hold the device name */
const char user_ble_device_name[] = "nimble_hr";
/* Hold the device appearance */
uint16_t user_ble_appearance = BLE_APPEARANCE_HEART_RATE_SENSOR_HEART_RATE_BELT;

/* Hold LED status */
extern volatile uint32_t led_status;

/* Functions */
extern void nimble_port_freertos_init(TaskFunction_t host_task_fn);
void user_advertise_init(void);

/**
 * @brief BLE gap event handler.
 * @param event Pointer to the event(event type + event data)
 * @param arg 
 * @param None
 */
static int user_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) 
    {
        case BLE_GAP_EVENT_CONNECT:
            printf("[connect] status:%d|conn_handle:%d\r\n",
                                    event->connect.status,
                                    event->connect.conn_handle);
            if (event->connect.status != 0)
            {
                /* Connection failed; resume advertising */
                user_advertise_init();
            }

            /* Change LED status */
            nrf_gpio_pin_set(LED_3);
            led_status &= ~BOARD_LED3_LIGHT;
            led_status |= BOARD_LED2_LIGHT;

            break;
       case BLE_GAP_EVENT_DISCONNECT:
            printf("[disconnect] reason:%d)\r\n",
                                    event->disconnect.reason);
            /* Connection terminated; resume advertising */
            user_advertise_init();

            /* Change LED status */
            nrf_gpio_pin_set(LED_2);
            led_status &= ~BOARD_LED2_LIGHT;
            led_status |= BOARD_LED3_LIGHT;

            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            printf("[adv end]\r\n");
            user_advertise_init();

            break;

        case BLE_GAP_EVENT_SUBSCRIBE:

            hr_service_subscribe_handler(event, arg);

            break;

        default:
            break;
    }

    return 0;
}

/**
 * @brief BLE advertise configuration.
 * @param None
 * @param None
 */
void user_advertise_init(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;

    /*
     *  Set the advertisement data included in our advertisements:
     *     o Flags
     *     o Advertising tx power
     *     o Device name
     *     o Appearance
     */
    memset(&fields, 0, sizeof(fields));

    /*
     * Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    fields.flags = BLE_HS_ADV_F_BREDR_UNSUP|BLE_HS_ADV_F_DISC_GEN;
    
    /*
     * Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)user_ble_device_name;
    fields.name_len = strlen(user_ble_device_name);
    fields.name_is_complete = 1;
    
    fields.appearance_is_present = 1;
    fields.appearance           = user_ble_appearance;

    ble_gap_adv_set_fields(&fields);

    /* Begin advertising */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    printf("[adv start] conn_mode:%d|disc_mode:%d\r\n",
                                adv_params.conn_mode,
                                adv_params.disc_mode);
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, user_ble_gap_event, NULL);
}

/**
 * @brief BLE host configuration synchronous callback.
 * @param None
 * @param None
 */
static void user_ble_sync_callback(void)
{
    int rc;

    /* Use privacy */
    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);

    /* Begin advertising */
    user_advertise_init();
}

/**
 * @brief An optional callback that gets executed upon registration of each GATT
 * resource (service, characteristic, or descriptor).
 * @param None
 * @param None
 */
void user_gatt_register_callback(struct ble_gatt_register_ctxt *ctxt,
                                  void *arg)
{
    uint8_t reg_type = ctxt->op;

    switch (reg_type)
    {
        case BLE_GATT_REGISTER_OP_SVC:
        {
            /* Read the registered service */
            printf("[svc reg    ] svc_uuid:%04X | handle:%04X\r\n",
                                        ble_uuid_u16(ctxt->svc.svc_def->uuid),
                                        ctxt->svc.handle);
            break;
        }
        case BLE_GATT_REGISTER_OP_CHR:
        {
            /* Read the registered characteristic */
            printf("[chr reg    ] chr_uuid:%04X "
                                        "| def_handle:%04X "
                                        "| val_handle:%04X\r\n",
                                        ble_uuid_u16(ctxt->chr.chr_def->uuid),
                                        ctxt->chr.def_handle,
                                        ctxt->chr.val_handle);
            break;
        }
        case BLE_GATT_REGISTER_OP_DSC:
        {
            /* Read the registered characteristic description */
            printf("[chr des reg] des_uuid:%04X "
                                        "| def_handle:%04X\r\n",
                                        ble_uuid_u16(ctxt->dsc.dsc_def->uuid),
                                        ctxt->dsc.handle);
            break;
        }
        default:
            break;
    }
}

/**
 * @brief BLE host function entry.
 * @param None
 * @param None
 */
void user_ble_host_entry(void *arg)
{
    /* Debug */
    printf("[Host entry]\r\n");

    /* Set the host synchronous callback */
    /* This callback is executed when the host and controller become synced. */
    ble_hs_cfg.sync_cb              = user_ble_sync_callback;
    ble_hs_cfg.gatts_register_cb    = user_gatt_register_callback;

    /* Add GAP servicce */
    gap_svr_init();
    /* Add heart-rate service */
    hr_svr_init();

    nimble_port_run();
}

/**
 * @brief Main.
 * @param None
 * @param None
 */
int main(void)
{
    /* Board init */
    board_init();

    /* Init the BLE stack */
    nimble_port_init();
    nimble_port_freertos_init(user_ble_host_entry);

    printf("[System start]:\r\n");
    /* Start FreeRTOS scheduler. */
    vTaskStartScheduler();

    while(1)
    {}
}


