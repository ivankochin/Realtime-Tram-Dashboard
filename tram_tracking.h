void tram_tracking_setup();

void tram_tracking_update_location(const char* tram_id, const char* location);
void tram_tracking_update_passenger_count(const char* tram_id, unsigned short passenger_count);

void tram_tracking_print_current_status();

void tram_tracking_destroy();
