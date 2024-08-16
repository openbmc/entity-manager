diff --git a/configurations/yosemite4_spiderboard_adc_max_pwr_ti.json b/configurations/yosemite4_spiderboard_adc_max_pwr_ti.json
index 74bcec4..b962e9a 100644
--- a/configurations/yosemite4_spiderboard_adc_max_pwr_ti.json
+++ b/configurations/yosemite4_spiderboard_adc_max_pwr_ti.json
@@ -1,537 +1,456 @@
 {
     "Exposes": [
         {
-            "Address": "0x73",
-            "Bus": "$bus",
-            "ChannelNames": [
-                "ADC_33_35",
-                "ADC_34_35"
-            ],
-            "Name": "Spider Board ADC Mux",
-            "Type": "PCA9544Mux"
-        },
-        {
-            "Address": "0x35",
-            "Bus": 33,
+            "Address": "0x40",
+            "Bus": 14,
             "Labels": [
-                "in_voltage0",
-                "in_voltage1",
-                "in_voltage2",
-                "in_voltage3",
-                "in_voltage4"
+                "vout1",
+                "iout1",
+                "pout1"
             ],
-            "Name": "SPIDER_ADC_33_35",
+            "Name": "SPIDER_INA233_12V_STBY",
             "Thresholds": [
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage0",
+                    "Label": "iout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 13.74
+                    "Value": 1.5
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage0",
+                    "Label": "iout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 13.176
+                    "Value": 0.88
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage0",
+                    "Label": "iout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 13.054
+                    "Value": 0.84
                 },
                 {
-                    "Direction": "less than",
-                    "Label": "in_voltage0",
-                    "Name": "lower non critical",
-                    "Severity": 0,
-                    "Value": 11.346
+                    "Direction": "greater than",
+                    "Label": "pout1",
+                    "Name": "upper non recoverable",
+                    "Severity": 4,
+                    "Value": 18.75
                 },
                 {
-                    "Direction": "less than",
-                    "Label": "in_voltage0",
-                    "Name": "lower critical",
+                    "Direction": "greater than",
+                    "Label": "pout1",
+                    "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 11.224
+                    "Value": 11
                 },
                 {
-                    "Direction": "less than",
-                    "Label": "in_voltage0",
-                    "Name": "lower non recoverable",
-                    "Severity": 4,
-                    "Value": 10.32
+                    "Direction": "greater than",
+                    "Label": "pout1",
+                    "Name": "upper non critical",
+                    "Severity": 0,
+                    "Value": 10.5
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 3.8
+                    "Value": 14.333
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 3.564
+                    "Value": 13.176
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 3.531
+                    "Value": 13.054
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "lower non critical",
                     "Severity": 0,
-                    "Value": 3.069
+                    "Value": 11.346
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "lower critical",
                     "Severity": 1,
-                    "Value": 3.036
+                    "Value": 11.224
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "lower non recoverable",
                     "Severity": 4,
-                    "Value": 2.8
-                },
+                    "Value": 10.091
+                }
+            ],
+            "Type": "INA233",
+            "iout1_Name": "SPIDER_INA233_12V_STBY_CURR_A",
+            "pout1_Name": "SPIDER_INA233_12V_STBY_PWR_W",
+            "vout1_Name": "SPIDER_INA233_12V_STBY_VOLT_V"
+        },
+        {
+            "Address": "0x41",
+            "Bus": 14,
+            "Labels": [
+                "vout1",
+                "iout1",
+                "pout1"
+            ],
+            "Name": "SPIDER_INA233_12V_NIC0",
+            "Thresholds": [
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage2",
+                    "Label": "iout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 13.74
+                    "Value": 5.4
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage2",
+                    "Label": "iout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 13.176
+                    "Value": 4.51
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage2",
+                    "Label": "iout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 13.054
+                    "Value": 4.305
                 },
                 {
-                    "Direction": "less than",
-                    "Label": "in_voltage2",
-                    "Name": "lower non critical",
-                    "Severity": 0,
-                    "Value": 11.346
+                    "Direction": "greater than",
+                    "Label": "pout1",
+                    "Name": "upper non recoverable",
+                    "Severity": 4,
+                    "Value": 67.5
                 },
                 {
-                    "Direction": "less than",
-                    "Label": "in_voltage2",
-                    "Name": "lower critical",
+                    "Direction": "greater than",
+                    "Label": "pout1",
+                    "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 11.224
+                    "Value": 56.375
                 },
                 {
-                    "Direction": "less than",
-                    "Label": "in_voltage2",
-                    "Name": "lower non recoverable",
-                    "Severity": 4,
-                    "Value": 10.32
+                    "Direction": "greater than",
+                    "Label": "pout1",
+                    "Name": "upper non critical",
+                    "Severity": 0,
+                    "Value": 53.8125
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage3",
+                    "Label": "vout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 3.8
+                    "Value": 14.333
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage3",
+                    "Label": "vout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 3.564
+                    "Value": 13.176
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage3",
+                    "Label": "vout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 3.531
+                    "Value": 13.054
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage3",
+                    "Label": "vout1",
                     "Name": "lower non critical",
                     "Severity": 0,
-                    "Value": 3.069
+                    "Value": 11.346
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage3",
+                    "Label": "vout1",
                     "Name": "lower critical",
                     "Severity": 1,
-                    "Value": 3.036
+                    "Value": 11.224
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage3",
+                    "Label": "vout1",
                     "Name": "lower non recoverable",
                     "Severity": 4,
-                    "Value": 2.8
+                    "Value": 10.091
                 }
             ],
-            "Type": "MAX11617",
-            "in_voltage0_Name": "SPIDER_ADC_12V_NIC0_VOLT_V",
-            "in_voltage0_Scale": 112.3596,
-            "in_voltage1_Name": "SPIDER_ADC_3V3_NIC0_VOLT_V",
-            "in_voltage1_Scale": 410.6776,
-            "in_voltage2_Name": "SPIDER_ADC_12V_NIC1_VOLT_V",
-            "in_voltage2_Scale": 112.3596,
-            "in_voltage3_Name": "SPIDER_ADC_3V3_NIC1_VOLT_V",
-            "in_volatge3_Scale": 410.6776
+            "Type": "INA233",
+            "iout1_Name": "SPIDER_INA233_12V_NIC0_CURR_A",
+            "pout1_Name": "SPIDER_INA233_12V_NIC0_PWR_W",
+            "vout1_Name": "SPIDER_INA233_12V_NIC0_VOLT_V"
         },
         {
-            "Address": "0x35",
-            "Bus": 34,
+            "Address": "0x42",
+            "Bus": 14,
             "Labels": [
-                "in_voltage0",
-                "in_voltage1",
-                "in_voltage2",
-                "in_voltage3",
-                "in_voltage8",
-                "in_Voltage9"
+                "vout1",
+                "iout1",
+                "pout1"
             ],
-            "Name": "SPIDER_ADC_34_35",
+            "Name": "SPIDER_INA233_12V_NIC1",
             "Thresholds": [
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage0",
+                    "Label": "iout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 13.74
+                    "Value": 5.4
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage0",
+                    "Label": "iout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 13.176
+                    "Value": 4.51
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage0",
+                    "Label": "iout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 13.054
+                    "Value": 4.305
                 },
                 {
-                    "Direction": "less than",
-                    "Label": "in_voltage0",
-                    "Name": "lower non critical",
-                    "Severity": 0,
-                    "Value": 11.346
+                    "Direction": "greater than",
+                    "Label": "pout1",
+                    "Name": "upper non recoverable",
+                    "Severity": 4,
+                    "Value": 67.5
                 },
                 {
-                    "Direction": "less than",
-                    "Label": "in_voltage0",
-                    "Name": "lower critical",
+                    "Direction": "greater than",
+                    "Label": "pout1",
+                    "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 11.224
+                    "Value": 56.375
                 },
                 {
-                    "Direction": "less than",
-                    "Label": "in_voltage0",
-                    "Name": "lower non recoverable",
-                    "Severity": 4,
-                    "Value": 10.32
+                    "Direction": "greater than",
+                    "Label": "pout1",
+                    "Name": "upper non critical",
+                    "Severity": 0,
+                    "Value": 53.8125
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 3.8
+                    "Value": 14.333
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 3.564
+                    "Value": 13.176
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 3.531
+                    "Value": 13.054
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "lower non critical",
                     "Severity": 0,
-                    "Value": 3.069
+                    "Value": 11.346
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "lower critical",
                     "Severity": 1,
-                    "Value": 3.036
+                    "Value": 11.224
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage1",
+                    "Label": "vout1",
                     "Name": "lower non recoverable",
                     "Severity": 4,
-                    "Value": 2.8
-                },
+                    "Value": 10.091
+                }
+            ],
+            "Type": "INA233",
+            "iout1_Name": "SPIDER_INA233_12V_NIC1_CURR_A",
+            "pout1_Name": "SPIDER_INA233_12V_NIC1_PWR_W",
+            "vout1_Name": "SPIDER_INA233_12V_NIC1_VOLT_V"
+        },
+        {
+            "Address": "0x43",
+            "Bus": 14,
+            "Labels": [
+                "vout1",
+                "iout1",
+                "pout1"
+            ],
+            "Name": "SPIDER_INA233_12V_NIC2",
+            "Thresholds": [
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage2",
+                    "Label": "iout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 13.74
+                    "Value": 5.4
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage2",
+                    "Label": "iout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 13.176
+                    "Value": 4.51
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage2",
+                    "Label": "iout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 13.054
-                },
-                {
-                    "Direction": "less than",
-                    "Label": "in_voltage2",
-                    "Name": "lower non critical",
-                    "Severity": 0,
-                    "Value": 11.346
-                },
-                {
-                    "Direction": "less than",
-                    "Label": "in_voltage2",
-                    "Name": "lower critical",
-                    "Severity": 1,
-                    "Value": 11.224
-                },
-                {
-                    "Direction": "less than",
-                    "Label": "in_voltage2",
-                    "Name": "lower non recoverable",
-                    "Severity": 4,
-                    "Value": 10.32
+                    "Value": 4.305
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage3",
+                    "Label": "pout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 3.8
+                    "Value": 67.5
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage3",
+                    "Label": "pout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 3.564
+                    "Value": 56.375
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage3",
+                    "Label": "pout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 3.531
-                },
-                {
-                    "Direction": "less than",
-                    "Label": "in_voltage3",
-                    "Name": "lower non critical",
-                    "Severity": 0,
-                    "Value": 3.069
-                },
-                {
-                    "Direction": "less than",
-                    "Label": "in_voltage3",
-                    "Name": "lower critical",
-                    "Severity": 1,
-                    "Value": 3.036
-                },
-                {
-                    "Direction": "less than",
-                    "Label": "in_voltage3",
-                    "Name": "lower non recoverable",
-                    "Severity": 4,
-                    "Value": 2.8
+                    "Value": 53.8125
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage8",
+                    "Label": "vout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
                     "Value": 14.333
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage8",
+                    "Label": "vout1",
                     "Name": "upper critical",
                     "Severity": 1,
                     "Value": 13.176
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "in_voltage8",
+                    "Label": "vout1",
                     "Name": "upper non critical",
                     "Severity": 0,
                     "Value": 13.054
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage8",
+                    "Label": "vout1",
                     "Name": "lower non critical",
                     "Severity": 0,
                     "Value": 11.346
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage8",
+                    "Label": "vout1",
                     "Name": "lower critical",
                     "Severity": 1,
                     "Value": 11.224
                 },
                 {
                     "Direction": "less than",
-                    "Label": "in_voltage8",
+                    "Label": "vout1",
                     "Name": "lower non recoverable",
                     "Severity": 4,
                     "Value": 10.091
-                },
-                {
-                    "Direction": "greater than",
-                    "Label": "in_voltage9",
-                    "Name": "upper non recoverable",
-                    "Severity": 4,
-                    "Value": 3.828
-                },
-                {
-                    "Direction": "greater than",
-                    "Label": "in_voltage9",
-                    "Name": "upper critical",
-                    "Severity": 1,
-                    "Value": 3.432
-                },
-                {
-                    "Direction": "greater than",
-                    "Label": "in_voltage9",
-                    "Name": "upper non critical",
-                    "Severity": 0,
-                    "Value": 3.399
-                },
-                {
-                    "Direction": "less than",
-                    "Label": "in_voltage9",
-                    "Name": "lower non critical",
-                    "Severity": 0,
-                    "Value": 3.201
-                },
-                {
-                    "Direction": "less than",
-                    "Label": "in_voltage9",
-                    "Name": "lower critical",
-                    "Severity": 1,
-                    "Value": 3.168
-                },
-                {
-                    "Direction": "less than",
-                    "Label": "in_voltage9",
-                    "Name": "lower non recoverable",
-                    "Severity": 4,
-                    "Value": 2.64
                 }
             ],
-            "Type": "MAX11617",
-            "in_voltage0_Name": "SPIDER_ADC_12V_NIC2_VOLT_V",
-            "in_voltage0_Scale": 112.3596,
-            "in_voltage1_Name": "SPIDER_ADC_3V3_NIC2_VOLT_V",
-            "in_voltage1_Scale": 410.6776,
-            "in_voltage2_Name": "SPIDER_ADC_12V_NIC3_VOLT_V",
-            "in_voltage2_Scale": 112.3596,
-            "in_voltage3_Name": "SPIDER_ADC_3V3_NIC3_VOLT_V",
-            "in_volatge3_Scale": 410.6776,
-            "in_voltage8_Name": "SPIDER_ADC_12V_MEDUSA_AUX_VOLT_V",
-            "in_volatge8_Scale": 112.3596,
-            "in_voltage9_Name": "SPIDER_ADC_3V3_AUX_VOLT_V",
-            "in_volatge9_Scale": 410.6776
+            "Type": "INA233",
+            "iout1_Name": "SPIDER_INA233_12V_NIC2_CURR_A",
+            "pout1_Name": "SPIDER_INA233_12V_NIC2_PWR_W",
+            "vout1_Name": "SPIDER_INA233_12V_NIC2_VOLT_V"
         },
         {
-            "Address": "0x40",
+            "Address": "0x44",
             "Bus": 14,
             "Labels": [
                 "vout1",
                 "iout1",
                 "pout1"
             ],
-            "Name": "SPIDER_INA233_12V_STBY",
+            "Name": "SPIDER_INA233_12V_NIC3",
             "Thresholds": [
                 {
                     "Direction": "greater than",
                     "Label": "iout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 1.5
+                    "Value": 5.4
                 },
                 {
                     "Direction": "greater than",
                     "Label": "iout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 0.88
+                    "Value": 4.51
                 },
                 {
                     "Direction": "greater than",
                     "Label": "iout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 0.84
+                    "Value": 4.305
                 },
                 {
                     "Direction": "greater than",
                     "Label": "pout1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 18.75
+                    "Value": 67.5
                 },
                 {
                     "Direction": "greater than",
                     "Label": "pout1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 11
+                    "Value": 56.375
                 },
                 {
                     "Direction": "greater than",
                     "Label": "pout1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 10.5
+                    "Value": 53.8125
                 },
                 {
                     "Direction": "greater than",
@@ -577,409 +496,490 @@
                 }
             ],
             "Type": "INA233",
-            "iout1_Name": "SPIDER_INA233_12V_STBY_CURR_A",
-            "pout1_Name": "SPIDER_INA233_12V_STBY_PWR_W",
-            "vout1_Name": "SPIDER_INA233_12V_STBY_VOLT_V"
+            "iout1_Name": "SPIDER_INA233_12V_NIC3_CURR_A",
+            "pout1_Name": "SPIDER_INA233_12V_NIC3_PWR_W",
+            "vout1_Name": "SPIDER_INA233_12V_NIC3_VOLT_V"
         },
         {
-            "Address": "0x41",
-            "Bus": 14,
+            "Address": "0x35",
+            "Bus": 33,
             "Labels": [
-                "vout1",
-                "iout1",
-                "pout1"
+                "in_voltage0",
+                "in_voltage1",
+                "in_voltage2",
+                "in_voltage3",
+                "in_voltage4"
             ],
-            "Name": "SPIDER_INA233_12V_NIC0",
+            "Name": "SPIDER_ADC_33_35",
             "Thresholds": [
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage0",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 5.4
+                    "Value": 13.74
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage0",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 4.51
+                    "Value": 13.176
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage0",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 4.305
+                    "Value": 13.054
                 },
                 {
-                    "Direction": "greater than",
-                    "Label": "pout1",
-                    "Name": "upper non recoverable",
-                    "Severity": 4,
-                    "Value": 67.5
+                    "Direction": "less than",
+                    "Label": "in_voltage0",
+                    "Name": "lower non critical",
+                    "Severity": 0,
+                    "Value": 11.346
                 },
                 {
-                    "Direction": "greater than",
-                    "Label": "pout1",
-                    "Name": "upper critical",
+                    "Direction": "less than",
+                    "Label": "in_voltage0",
+                    "Name": "lower critical",
                     "Severity": 1,
-                    "Value": 56.375
+                    "Value": 11.224
                 },
                 {
-                    "Direction": "greater than",
-                    "Label": "pout1",
-                    "Name": "upper non critical",
-                    "Severity": 0,
-                    "Value": 53.8125
+                    "Direction": "less than",
+                    "Label": "in_voltage0",
+                    "Name": "lower non recoverable",
+                    "Severity": 4,
+                    "Value": 10.32
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 14.333
+                    "Value": 3.8
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 13.176
+                    "Value": 3.564
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 13.054
+                    "Value": 3.531
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage1",
                     "Name": "lower non critical",
                     "Severity": 0,
-                    "Value": 11.346
+                    "Value": 3.069
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage1",
                     "Name": "lower critical",
                     "Severity": 1,
-                    "Value": 11.224
+                    "Value": 3.036
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage1",
                     "Name": "lower non recoverable",
                     "Severity": 4,
-                    "Value": 10.091
-                }
-            ],
-            "Type": "INA233",
-            "iout1_Name": "SPIDER_INA233_12V_NIC0_CURR_A",
-            "pout1_Name": "SPIDER_INA233_12V_NIC0_PWR_W",
-            "vout1_Name": "SPIDER_INA233_12V_NIC0_VOLT_V"
-        },
-        {
-            "Address": "0x42",
-            "Bus": 14,
-            "Labels": [
-                "vout1",
-                "iout1",
-                "pout1"
-            ],
-            "Name": "SPIDER_INA233_12V_NIC1",
-            "Thresholds": [
+                    "Value": 2.8
+                },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage2",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 5.4
+                    "Value": 13.74
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage2",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 4.51
+                    "Value": 13.176
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage2",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 4.305
+                    "Value": 13.054
                 },
                 {
-                    "Direction": "greater than",
-                    "Label": "pout1",
-                    "Name": "upper non recoverable",
-                    "Severity": 4,
-                    "Value": 67.5
+                    "Direction": "less than",
+                    "Label": "in_voltage2",
+                    "Name": "lower non critical",
+                    "Severity": 0,
+                    "Value": 11.346
                 },
                 {
-                    "Direction": "greater than",
-                    "Label": "pout1",
-                    "Name": "upper critical",
+                    "Direction": "less than",
+                    "Label": "in_voltage2",
+                    "Name": "lower critical",
                     "Severity": 1,
-                    "Value": 56.375
+                    "Value": 11.224
                 },
                 {
-                    "Direction": "greater than",
-                    "Label": "pout1",
-                    "Name": "upper non critical",
-                    "Severity": 0,
-                    "Value": 53.8125
+                    "Direction": "less than",
+                    "Label": "in_voltage2",
+                    "Name": "lower non recoverable",
+                    "Severity": 4,
+                    "Value": 10.32
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage3",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 14.333
+                    "Value": 3.8
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage3",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 13.176
+                    "Value": 3.564
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage3",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 13.054
+                    "Value": 3.531
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage3",
                     "Name": "lower non critical",
                     "Severity": 0,
-                    "Value": 11.346
+                    "Value": 3.069
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage3",
                     "Name": "lower critical",
                     "Severity": 1,
-                    "Value": 11.224
+                    "Value": 3.036
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage3",
                     "Name": "lower non recoverable",
                     "Severity": 4,
-                    "Value": 10.091
+                    "Value": 2.8
                 }
             ],
-            "Type": "INA233",
-            "iout1_Name": "SPIDER_INA233_12V_NIC1_CURR_A",
-            "pout1_Name": "SPIDER_INA233_12V_NIC1_PWR_W",
-            "vout1_Name": "SPIDER_INA233_12V_NIC1_VOLT_V"
+            "Type": "MAX11617",
+            "in_volatge3_Scale": 410.6776,
+            "in_voltage0_Name": "SPIDER_ADC_12V_NIC0_VOLT_V",
+            "in_voltage0_Scale": 112.3596,
+            "in_voltage1_Name": "SPIDER_ADC_3V3_NIC0_VOLT_V",
+            "in_voltage1_Scale": 410.6776,
+            "in_voltage2_Name": "SPIDER_ADC_12V_NIC1_VOLT_V",
+            "in_voltage2_Scale": 112.3596,
+            "in_voltage3_Name": "SPIDER_ADC_3V3_NIC1_VOLT_V"
         },
         {
-            "Address": "0x43",
-            "Bus": 14,
+            "Address": "0x35",
+            "Bus": 34,
             "Labels": [
-                "vout1",
-                "iout1",
-                "pout1"
+                "in_voltage0",
+                "in_voltage1",
+                "in_voltage2",
+                "in_voltage3",
+                "in_voltage8",
+                "in_Voltage9"
             ],
-            "Name": "SPIDER_INA233_12V_NIC2",
+            "Name": "SPIDER_ADC_34_35",
             "Thresholds": [
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage0",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 5.4
+                    "Value": 13.74
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage0",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 4.51
+                    "Value": 13.176
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage0",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 4.305
+                    "Value": 13.054
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage0",
+                    "Name": "lower non critical",
+                    "Severity": 0,
+                    "Value": 11.346
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage0",
+                    "Name": "lower critical",
+                    "Severity": 1,
+                    "Value": 11.224
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage0",
+                    "Name": "lower non recoverable",
+                    "Severity": 4,
+                    "Value": 10.32
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "pout1",
+                    "Label": "in_voltage1",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 67.5
+                    "Value": 3.8
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "pout1",
+                    "Label": "in_voltage1",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 56.375
+                    "Value": 3.564
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "pout1",
+                    "Label": "in_voltage1",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 53.8125
+                    "Value": 3.531
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage1",
+                    "Name": "lower non critical",
+                    "Severity": 0,
+                    "Value": 3.069
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage1",
+                    "Name": "lower critical",
+                    "Severity": 1,
+                    "Value": 3.036
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage1",
+                    "Name": "lower non recoverable",
+                    "Severity": 4,
+                    "Value": 2.8
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage2",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 14.333
+                    "Value": 13.74
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage2",
                     "Name": "upper critical",
                     "Severity": 1,
                     "Value": 13.176
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage2",
                     "Name": "upper non critical",
                     "Severity": 0,
                     "Value": 13.054
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage2",
                     "Name": "lower non critical",
                     "Severity": 0,
                     "Value": 11.346
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage2",
                     "Name": "lower critical",
                     "Severity": 1,
                     "Value": 11.224
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage2",
                     "Name": "lower non recoverable",
                     "Severity": 4,
-                    "Value": 10.091
-                }
-            ],
-            "Type": "INA233",
-            "iout1_Name": "SPIDER_INA233_12V_NIC2_CURR_A",
-            "pout1_Name": "SPIDER_INA233_12V_NIC2_PWR_W",
-            "vout1_Name": "SPIDER_INA233_12V_NIC2_VOLT_V"
-        },
-        {
-            "Address": "0x44",
-            "Bus": 14,
-            "Labels": [
-                "vout1",
-                "iout1",
-                "pout1"
-            ],
-            "Name": "SPIDER_INA233_12V_NIC3",
-            "Thresholds": [
+                    "Value": 10.32
+                },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage3",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 5.4
+                    "Value": 3.8
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage3",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 4.51
+                    "Value": 3.564
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "iout1",
+                    "Label": "in_voltage3",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 4.305
+                    "Value": 3.531
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage3",
+                    "Name": "lower non critical",
+                    "Severity": 0,
+                    "Value": 3.069
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage3",
+                    "Name": "lower critical",
+                    "Severity": 1,
+                    "Value": 3.036
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage3",
+                    "Name": "lower non recoverable",
+                    "Severity": 4,
+                    "Value": 2.8
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "pout1",
+                    "Label": "in_voltage8",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 67.5
+                    "Value": 14.333
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "pout1",
+                    "Label": "in_voltage8",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 56.375
+                    "Value": 13.176
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "pout1",
+                    "Label": "in_voltage8",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 53.8125
+                    "Value": 13.054
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage8",
+                    "Name": "lower non critical",
+                    "Severity": 0,
+                    "Value": 11.346
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage8",
+                    "Name": "lower critical",
+                    "Severity": 1,
+                    "Value": 11.224
+                },
+                {
+                    "Direction": "less than",
+                    "Label": "in_voltage8",
+                    "Name": "lower non recoverable",
+                    "Severity": 4,
+                    "Value": 10.091
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage9",
                     "Name": "upper non recoverable",
                     "Severity": 4,
-                    "Value": 14.333
+                    "Value": 3.828
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage9",
                     "Name": "upper critical",
                     "Severity": 1,
-                    "Value": 13.176
+                    "Value": 3.432
                 },
                 {
                     "Direction": "greater than",
-                    "Label": "vout1",
+                    "Label": "in_voltage9",
                     "Name": "upper non critical",
                     "Severity": 0,
-                    "Value": 13.054
+                    "Value": 3.399
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage9",
                     "Name": "lower non critical",
                     "Severity": 0,
-                    "Value": 11.346
+                    "Value": 3.201
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage9",
                     "Name": "lower critical",
                     "Severity": 1,
-                    "Value": 11.224
+                    "Value": 3.168
                 },
                 {
                     "Direction": "less than",
-                    "Label": "vout1",
+                    "Label": "in_voltage9",
                     "Name": "lower non recoverable",
                     "Severity": 4,
-                    "Value": 10.091
+                    "Value": 2.64
                 }
             ],
-            "Type": "INA233",
-            "iout1_Name": "SPIDER_INA233_12V_NIC3_CURR_A",
-            "pout1_Name": "SPIDER_INA233_12V_NIC3_PWR_W",
-            "vout1_Name": "SPIDER_INA233_12V_NIC3_VOLT_V"
+            "Type": "MAX11617",
+            "in_volatge3_Scale": 410.6776,
+            "in_volatge8_Scale": 112.3596,
+            "in_volatge9_Scale": 410.6776,
+            "in_voltage0_Name": "SPIDER_ADC_12V_NIC2_VOLT_V",
+            "in_voltage0_Scale": 112.3596,
+            "in_voltage1_Name": "SPIDER_ADC_3V3_NIC2_VOLT_V",
+            "in_voltage1_Scale": 410.6776,
+            "in_voltage2_Name": "SPIDER_ADC_12V_NIC3_VOLT_V",
+            "in_voltage2_Scale": 112.3596,
+            "in_voltage3_Name": "SPIDER_ADC_3V3_NIC3_VOLT_V",
+            "in_voltage8_Name": "SPIDER_ADC_12V_MEDUSA_AUX_VOLT_V",
+            "in_voltage9_Name": "SPIDER_ADC_3V3_AUX_VOLT_V"
+        },
+        {
+            "Address": "0x73",
+            "Bus": "$bus",
+            "ChannelNames": [
+                "ADC_33_35",
+                "ADC_34_35"
+            ],
+            "Name": "Spider Board ADC Mux",
+            "Type": "PCA9544Mux"
         },
         {
             "Class": "temp",
