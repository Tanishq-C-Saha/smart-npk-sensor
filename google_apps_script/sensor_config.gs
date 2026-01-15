function doGet() {
  const config = {
    interval_sec: 60,     //  1 minute
    start_time: "9:30",   // Start working
    end_time: "17:00"      // Go to sleep
  };

  return ContentService
    .createTextOutput(JSON.stringify(config))
    .setMimeType(ContentService.MimeType.JSON);
}


