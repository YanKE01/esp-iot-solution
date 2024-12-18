ADC 扩展量程方案
====================
:link_to_translation:`en:[English]`

ESP32-S3 ADC 扩展量程方案
----------------------------

ESP32-S3 ADC 最大有效量程为 0 ~ 3100 mV，通过外部分压电路，能够满足大部分的 ADC 按键、电池电压检测等功能。但是对于 NTC 等应用场景，可能需要支持满量程（0 ~ 3300 mV）测量。
ESP32-S3 可通过配置寄存器调整 ADC 的偏置电压，再结合高电压区域非线性补偿方法，我们可以实现对 ESP32-S3 量程的扩展。

实现量程扩展的过程为：

1. 使用默认偏置，测量一次电压值
2. 当测量电压值小于 2900 mV，直接输出测量电压值作为测量结果
3. 当测量电压值大于 2900 mV，提升偏置电压，进行二次测量，并进行非线性修正计算，输出修正值作为测量结果
4. 测量完成，恢复偏置电压

因此，每次 ADC 测量，期间可能读取 1~2 次实际电压值。对于大部分的应用场景，该方案引入的测量延迟可以忽略不计。

Patch 使用方法
-------------------

基于 ESP-IDF ``v4.4.8`` 的 Patch 使用方法
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. 确认 ESP-IDF 已经 ``checkout`` 到 ``v4.4.8``
2. 下载 :download:`esp32s3_adc_range_to_3300.patch <../../_static/esp32s3_adc_range_to_3300.patch>` 文件
3. 使用指令 ``git am --signoff < esp32s3_adc_range_to_3300.patch`` 将 Patch 应用到 IDF 中
4. 请注意， 该方案仅对 ``esp_adc_cal_get_voltage`` 接口有效，用户可直接调用该接口获取扩展后的读数

基于 ESP-IDF ``v5.3.1`` 的 Patch 使用方法
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. 确认 ESP-IDF 已经 ``checkout`` 到 ``v5.3.1``
2. 下载 :download:`esp32s3_adc_range_to_3300_v531.patch <../../_static/esp32s3_adc_range_to_3300_v531.patch>` 文件
3. 使用指令 ``git am --signoff < esp32s3_adc_range_to_3300_v531.patch`` 将 Patch 应用到 IDF 中
4. 请注意， 该方案仅对 ``adc_oneshot_get_calibrated_result`` 接口有效，用户可直接调用该接口获取扩展后的读数

API 使用说明
--------------

对于不同版本的 ESP-IDF，获取 ADC 量程扩展后的电压值方法有所不同：

- ESP-IDF ``v4.4.8``

  1. 如果想要读取量程扩展后的电压值，用户必须使用 ``esp_adc_cal_get_voltage`` 直接获取 ``ADC1`` 或 ``ADC2`` 的通道电压
  2. ESP-IDF ``v4.4.8`` ADC 其它 API 不受影响，读取的结果和默认结果一致

- ESP-IDF ``v5.3.1``

  1. 如果想要读取量程扩展后的电压值，用户必须使用 ``adc_oneshot_get_calibrated_result`` 直接获取 ``ADC1`` 或 ``ADC2`` 的通道电压
  2. ESP-IDF ``v5.3.1`` ADC 其它 API 不受影响，读取的结果和默认结果一致
