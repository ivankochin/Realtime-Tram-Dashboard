#include <khash.h> // Discuss selection of hash table implementation
#include <stdio.h>
#include <string.h>

#define LOCATION_SIZE 256
#define UNK_PASS_COUNT USHRT_MAX

typedef struct {
    char location[LOCATION_SIZE];
    unsigned short passenger_count;
} tram_info;

KHASH_MAP_INIT_STR(trams_storage_table, tram_info);

khash_t(trams_storage_table) *trams_storage;

void tram_tracking_setup() {
    trams_storage = kh_init(trams_storage_table);
}

void tram_tracking_update_location(const char* tram_id, const char* location) {
    int ret;
    khint_t k = kh_put(trams_storage_table, trams_storage, tram_id, &ret);
    if (ret) { // new key
        kh_value(trams_storage, k).passenger_count = UNK_PASS_COUNT;
        kh_key(trams_storage, k) = strdup(tram_id);
    }
    snprintf(kh_val(trams_storage, k).location, LOCATION_SIZE, "%s", location);
}

void tram_tracking_update_passenger_count(const char* tram_id, unsigned short passenger_count) {
    int ret;
    khint_t k = kh_put(trams_storage_table, trams_storage, tram_id, &ret);
    if (ret) { // new key
        snprintf(kh_val(trams_storage, k).location, LOCATION_SIZE, "unknown");
        kh_key(trams_storage, k) = strdup(tram_id);
    }
    kh_value(trams_storage, k).passenger_count = passenger_count;
}

void tram_tracking_print_current_status() {
    char pass_count_str[8];
    const char* tram_id;
    tram_info info;

    printf("\033[H\033[J"); // clear screen
    kh_foreach(trams_storage, tram_id, info, { // Discuss whether it is important to report trams in particular order
        snprintf(pass_count_str, 8, "%d", info.passenger_count);

        printf("Tram %s:\n", tram_id);
        printf("    Location: %s\n", info.location);
        printf("    Passenger Count: %s\n",
               (info.passenger_count == UNK_PASS_COUNT) ? "unknown" : pass_count_str);
    });
}

void tram_tracking_destroy() {
    khint_t k;
    for (k = kh_begin(trams_storage); k != kh_end(trams_storage); ++k) {
        if (kh_exist(trams_storage, k)) {
            free((char*)kh_key(trams_storage, k));
        }
    }
    kh_destroy(trams_storage_table, trams_storage);
}
