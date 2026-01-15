const SPREADSHEET_ID = "your_active_spreadsheet_id";

function doPost(e) {
  const SHEET_NAME = "NPKFakeSensorData3";
  const ss = SpreadsheetApp.openById(SPREADSHEET_ID);

  let sheet = ss.getSheetByName(SHEET_NAME);
  if (!sheet) {
    sheet = ss.insertSheet(SHEET_NAME);
    sheet.appendRow([
      "timestamp",
      "N",
      "P",
      "K",
      "temperature",
      "moisture",
      "pH",
      "conductivity"
    ]);
  }

  const data = JSON.parse(e.postData.contents);

  sheet.appendRow([
    new Date(),
    data.N,
    data.P,
    data.K,
    data.temperature,
    data.moisture,
    data.pH,
    data.conductivity
  ]);

  return ContentService
    .createTextOutput(JSON.stringify({ status: "ok" }))
    .setMimeType(ContentService.MimeType.JSON);
}

