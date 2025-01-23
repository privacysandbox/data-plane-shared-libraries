// Replace with id of generated .json file
var JSON_GOOGLE_DRIVE_ID = '1AaLLmrZRT5EWAsfGh0mJPrdMNEXG-8lj';
var SHEET_NAME = 'Sheet8';

/**
 * Converts a traffic generator JSONL file to a Google Spreadsheet.
 *
 * Steps to use this script:
 * 1. Upload your .jsonl file to Google Drive
 *    - Open drive.google.com
 *    - Click "New" -> "File upload"
 *    - Select your .jsonl file
 *    - Once uploaded, click the file and copy the ID from the URL
 *      (e.g., from "https://drive.google.com/file/d/1AaLLmrZRT5EWAsfGh0mJPrdMNEXG-8lj/view",
 *       the ID is "1AaLLmrZRT5EWAsfGh0mJPrdMNEXG-8lj")
 *
 * 2. Create a new Google Spreadsheet
 *    - Go to sheets.google.com
 *    - Click "Blank" to create a new spreadsheet
 *    - Note the name of the sheet you want to write to (default is "Sheet1")
 *
 * 3. Set up the script
 *    - In your spreadsheet, click "Extensions" -> "Apps Script"
 *    - Copy the contents of this file into the script editor
 *    - Update JSON_GOOGLE_DRIVE_ID with your file ID from step 1
 *    - Update SHEET_NAME with your sheet name from step 2
 *
 * 4. Run the script
 *    - Click the "Run" button (play icon)
 *    - Grant necessary permissions when prompted
 *    - The data will be populated in your spreadsheet
 *
 * The JSONL file should contain one JSON object per line, with each object having the following structure:
 *
 * Example JSONL content:
 * Contents of a jsonl file, foobar.jsonl:
 * {"runId":"","params":{"burstSize":"1","queriesPerSecond":220,"queryCount":500,"sandboxEnabled":false,"numWorkers":100},"statistics":{"totalElapsedTime":"2.269842556s","totalInvocationCount":"500","lateCount":"0","lateBurstPct":0},"burstLatencies":{"count":"500","min":"0.000987850s","p50":"0.001392840s","p90":"0.001615240s","p95":"0.001684730s","p99":"0.001868450s","max":"0.003158330s"},"invocationLatencies":{"count":"500","min":"0.000977020s","p50":"0.001350550s","p90":"0.001573800s","p95":"0.001643490s","p99":"0.001828080s","max":"0.003066520s"}}
 * {"runId":"","params":{"burstSize":"2","queriesPerSecond":230,"queryCount":500,"sandboxEnabled":false,"numWorkers":100},"statistics":{"totalElapsedTime":"2.170732601s","totalInvocationCount":"500","lateCount":"0","lateBurstPct":0},"burstLatencies":{"count":"500","min":"0.001017490s","p50":"0.001354270s","p90":"0.001567520s","p95":"0.001619590s","p99":"0.001766210s","max":"0.003402050s"},"invocationLatencies":{"count":"500","min":"0.000983400s","p50":"0.001314651s","p90":"0.001523470s","p95":"0.001581490s","p99":"0.001711401s","max":"0.003345240s"}}
 * {"runId":"","params":{"burstSize":"3","queriesPerSecond":240,"queryCount":500,"sandboxEnabled":false,"numWorkers":100},"statistics":{"totalElapsedTime":"2.080588877s","totalInvocationCount":"500","lateCount":"1","lateBurstPct":0.2},"burstLatencies":{"count":"500","min":"0.000977889s","p50":"0.001279720s","p90":"0.001498090s","p95":"0.001566740s","p99":"0.001755370s","max":"0.005394200s"},"invocationLatencies":{"count":"500","min":"0.000950211s","p50":"0.001245270s","p90":"0.001454911s","p95":"0.001523520s","p99":"0.001715940s","max":"0.005004560s"}}
 */
function ConvertTrafficGeneratorJsonlToSpreadSheet() {
  var file = DriveApp.getFileById(JSON_GOOGLE_DRIVE_ID);
  var fileContent = file.getBlob().getDataAsString();

  // Split the content by newlines and parse each line as JSON
  var jsonData = fileContent.split('\n')
    .filter(line => line.trim() !== '') // Remove empty lines
    .map(line => JSON.parse(line));

  var ss = SpreadsheetApp.getActiveSpreadsheet();
  // Replace with Sheet Name to be updated
  var sheet = ss.getSheetByName(SHEET_NAME);

  // 3. Define the desired column headers and their corresponding JSON paths
  let columnMapping = [
    { header: 'Run ID', path: 'runId' },
    { header: 'Burst Size', path: 'params.burstSize' },
    { header: 'QPS', path: 'params.queriesPerSecond' },
    { header: 'Num Bursts', path: 'params.queryCount' },
    { header: 'Num Workers', path: 'params.numWorkers' },
    { header: 'Total Elapsed Time (s)', path: 'statistics.totalElapsedTime', transform: (val) => val.replace('s', '') },
    { header: 'Total Invocation Count', path: 'statistics.totalInvocationCount' },
    { header: 'Late Count', path: 'statistics.lateCount' },
    { header: 'Late Burst Pct', path: 'statistics.lateBurstPct' },
    { header: 'Burst Latencies min (ms)', path: 'burstLatencies.min', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies p50 (ms)', path: 'burstLatencies.p50', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies p90 (ms)', path: 'burstLatencies.p90', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies p95 (ms)', path: 'burstLatencies.p95', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies p99 (ms)', path: 'burstLatencies.p99', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies max (ms)', path: 'burstLatencies.max', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Invocation Latencies min (ms)', path: 'invocationLatencies.min', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Invocation Latencies p50 (ms)', path: 'invocationLatencies.p50', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Invocation Latencies p90 (ms)', path: 'invocationLatencies.p90', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Invocation Latencies p95 (ms)', path: 'invocationLatencies.p95', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Invocation Latencies p99 (ms)', path: 'invocationLatencies.p99', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Invocation Latencies max (ms)', path: 'invocationLatencies.max', transform: (val) => secondsToMilliseconds(val) },
  ];

  if (jsonData[0].outputLatencies?.min) {
    columnMapping.push({ header: 'Output Latencies min (ms)', path: 'outputLatencies.min', transform: (val) => secondsToMilliseconds(val) });
    columnMapping.push({ header: 'Output Latencies p50 (ms)', path: 'outputLatencies.p50', transform: (val) => secondsToMilliseconds(val) });
    columnMapping.push({ header: 'Output Latencies p90 (ms)', path: 'outputLatencies.p90', transform: (val) => secondsToMilliseconds(val) });
    columnMapping.push({ header: 'Output Latencies p95 (ms)', path: 'outputLatencies.p95', transform: (val) => secondsToMilliseconds(val) });
    columnMapping.push({ header: 'Output Latencies p99 (ms)', path: 'outputLatencies.p99', transform: (val) => secondsToMilliseconds(val) });
    columnMapping.push({ header: 'Output Latencies max (ms)', path: 'outputLatencies.max', transform: (val) => secondsToMilliseconds(val) });
  }

  // 4. Prepare the data
  var data = [];
  data.push(columnMapping.map(col => col.header)); // Add headers

  jsonData.forEach(function(record) {
    var row = columnMapping.map(col => {
      // Apply transformation function if exists
      return col.transform ? col.transform(getNestedValue(record, col.path)) : getNestedValue(record, col.path);
    });
    data.push(row);
  });

  // 5. Write the data to the sheet
  sheet.getRange(1, 1, data.length, data[0].length).setValues(data);
}

function secondsToMilliseconds(secondsString) {
  // Extract the numeric part from the string
  const seconds = parseFloat(secondsString.replace('s', ''));

  // Convert seconds to milliseconds
  const milliseconds = seconds * 1000;

  return milliseconds.toFixed(3);
}

// Helper function to get nested object values using dot notation path
function getNestedValue(obj, path) {
  return path.split('.').reduce((current, key) =>
    current && current[key] !== undefined ? current[key] : '', obj);
}
