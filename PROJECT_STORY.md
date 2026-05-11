# 專案 1：Linux Virtual Sensor Character Driver

## 一句話

我做了一個 Linux kernel character device driver，把虛擬感測器資料透過 `/dev/sp_virt_sensor` 暴露給 user space。

## 功能

- Kernel module init/exit
- 自動建立 `/dev/sp_virt_sensor`
- `write()`：user space 寫入一筆 sensor sample
- `read()`：blocking 等待並讀出 sample
- `ioctl()`：查詢 buffer 中目前 sample 數量
- `poll()`：支援 select/poll 等待資料
- mutex 保護 ring buffer
- wait queue 喚醒 blocked reader

## 為什麼這能對應職缺

職缺要求 embedded Linux system/device driver。這個專案練到 driver 的最小核心能力：

- user/kernel interface
- file operations
- concurrency control
- blocking I/O
- debug with dmesg

## 面試講法

> 我做了一個 virtual sensor char driver。User program 可透過 write 模擬硬體 sample 進來，driver 用 ring buffer 保存；read 如果沒有資料會進入 blocking wait queue，有資料後被喚醒；ioctl 可查詢 queue depth；poll 讓應用可以用事件式方式等待資料。我在這個專案中練到 character device 註冊、file_operations、copy_to_user/copy_from_user、mutex 與 wait queue。

## 可被追問的點

### 如果接真硬體，下一步怎麼做？

改成 platform driver，透過 device tree compatible match；probe 時讀 `reg` 和 `interrupts`，使用 `devm_ioremap_resource()` map MMIO，用 `devm_request_irq()` 註冊 ISR。

### ISR 來資料時怎麼處理？

ISR 只讀取狀態、清 interrupt、把資料放入 ring buffer 或排 work，再 wake up wait queue。不能在 ISR 裡做可能 sleep 的事。

### 多 process 同時讀寫怎麼辦？

用 mutex 保護 shared buffer；若需要 per-open 狀態，可在 open 建立 private data 並掛到 `file->private_data`。

