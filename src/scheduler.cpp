#include "scheduler.h"

#include "bot_screens.h"
#include "geomagnetic_client.h"
#include "runtime_state.h"
#include "storage.h"
#include "time_utils.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <string>
#include <thread>

using namespace std;

bool kp_available(double kp);

void scheduler() {
    map<long long, bool> morning_sent;
    time_t last_check_slot = -1;
    int morning_day = -1;

    while (true) {
        tm ltm = get_minsk_time();

        const int day_key = (ltm.tm_year + 1900) * 1000 + ltm.tm_yday;
        if (morning_day != day_key) {
            morning_sent.clear();
            morning_day = day_key;
        }
        if (ltm.tm_hour == 9 && ltm.tm_min < 5) {
            for (long long uid : active_user_snapshot()) {
                if (!morning_sent[uid]) {
                    send_morning_report(uid, 0, true);
                    morning_sent[uid] = true;
                    this_thread::sleep_for(chrono::milliseconds(100));
                }
            }
        }
        if (ltm.tm_hour == 10) {
            morning_sent.clear();
        }

        const time_t check_slot = time(nullptr) / 1200;
        if (check_slot != last_check_slot) {
            last_check_slot = check_slot;
            double current_kp = fetch_current_kp();
            if (!kp_available(current_kp)) {
                this_thread::sleep_for(chrono::seconds(30));
                continue;
            }
            time_t now_ts = time(nullptr);

            bool should_update_alert = false;
            string alert_log;

            if (current_kp >= 5.0) {
                if (!storm_alert_active || last_alert_time == 0) {
                    should_update_alert = true;
                    alert_log = "старт бури";
                } else if (current_kp >= last_alert_kp + 0.9) {
                    should_update_alert = true;
                    alert_log = "усиление бури";
                } else if ((last_alert_kp - current_kp) >= 0.9 && (now_ts - last_alert_time) > 3600) {
                    should_update_alert = true;
                    alert_log = "ослабление бури";
                } else if ((now_ts - last_alert_time) > 10800) {
                    should_update_alert = true;
                    alert_log = "плановое обновление бури";
                }
            } else if (storm_alert_active) {
                should_update_alert = true;
                alert_log = "буря затихла";
            }

            if (should_update_alert) {
                for (long long uid : active_user_snapshot()) {
                    if (is_notifications_enabled(uid)) {
                        show_alert_screen(uid, current_kp, true);
                        this_thread::sleep_for(chrono::milliseconds(100));
                    }
                }

                last_alert_kp = current_kp;
                last_alert_time = current_kp >= 5.0 ? now_ts : 0;
                storm_alert_active = current_kp >= 5.0;

                cout << "⚠️ Обновлён storm alert (" << alert_log << "), Kp: " << current_kp << endl;
            }
        }

        this_thread::sleep_for(chrono::seconds(30));
    }
}

