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
 * {"info":{"platform":"GCP CS","image":"privacysandbox/roma-byob/traffic-generator:v1-250qps-84w-bs14-nq2000-minimal-root","instance":"gcp-rfys-4493b3d63c","machineType":"n2d-standard-64","zone":"us-central1-a","hardwareThreadCount":64,"memory":0,"linuxKernel":"6.1.112+","rlimitSigpending":{"soft":1027030,"hard":1027030}},"runId":"00000000-0000-03DF-EB90-CE1531261918","params":{"burstSize":"14","queriesPerSecond":250,"queryCount":2000,"sandboxEnabled":false,"numWorkers":84},"statistics":{"totalElapsedTime":"0.004500516s","totalInvocationCount":"28","lateCount":"0","lateBurstPct":0,"failureCount":"0","failurePct":0},"burstLatencies":{"count":"2","min":"0.000270170s","p50":"0.000954637s","p90":"0.000954637s","p95":"0.000954637s","p99":"0.000954637s","max":"0.000954637s"},"invocationLatencies":{"count":"28","min":"0.000091680s","p50":"0.000152810s","p90":"0.000316689s","p95":"0.000476028s","p99":"0.000496278s","max":"0.000496278s"}}
 * {"info":{"platform":"GCP CS","image":"privacysandbox/roma-byob/traffic-generator:v1-250qps-84w-bs14-nq2000-minimal-root","instance":"gcp-rfys-83d9d694ba","machineType":"n2d-standard-32","zone":"us-central1-c","hardwareThreadCount":32,"memory":0,"linuxKernel":"6.1.112+","rlimitSigpending":{"soft":511359,"hard":511359}},"runId":"00000000-0000-03F3-9E2A-F0DD2B826EB6","params":{"burstSize":"14","queriesPerSecond":250,"queryCount":2000,"sandboxEnabled":false,"numWorkers":84},"statistics":{"totalElapsedTime":"0.004466092s","totalInvocationCount":"28","lateCount":"0","lateBurstPct":0,"failureCount":"0","failurePct":0},"burstLatencies":{"count":"2","min":"0.000244840s","p50":"0.000260570s","p90":"0.000260570s","p95":"0.000260570s","p99":"0.000260570s","max":"0.000260570s"},"invocationLatencies":{"count":"28","min":"0.000076530s","p50":"0.000170530s","p90":"0.000346900s","p95":"0.000533s","p99":"0.000555590s","max":"0.000555590s"}}
 * {"info":{"platform":"GCP CS","image":"privacysandbox/roma-byob/traffic-generator:v1-500qps-84w-bs14-nq2000-minimal-root","instance":"gcp-rfys-b936edff84","machineType":"n2d-standard-64","zone":"us-central1-a","hardwareThreadCount":64,"memory":0,"linuxKernel":"6.1.112+","rlimitSigpending":{"soft":1027030,"hard":1027030}},"runId":"00000000-0000-0405-81FD-62C62A9BEFE0","params":{"burstSize":"14","queriesPerSecond":500,"queryCount":2000,"sandboxEnabled":false,"numWorkers":84},"statistics":{"totalElapsedTime":"0.002845266s","totalInvocationCount":"28","lateCount":"0","lateBurstPct":0,"failureCount":"0","failurePct":0},"burstLatencies":{"count":"2","min":"0.000639079s","p50":"0.000961979s","p90":"0.000961979s","p95":"0.000961979s","p99":"0.000961979s","max":"0.000961979s"},"invocationLatencies":{"count":"28","min":"0.000081300s","p50":"0.000133290s","p90":"0.000290460s","p95":"0.000435409s","p99":"0.000511709s","max":"0.000511709s"}}
 */
function ConvertTrafficGeneratorJsonlToSpreadSheet() {
  var file = DriveApp.getFileById(JSON_GOOGLE_DRIVE_ID);
  var fileContent = file.getBlob().getDataAsString();

  // Split the content by newlines and parse each line as JSON
  var jsonData = fileContent
    .split('\n')
    .filter((line) => line.trim() !== '') // Remove empty lines
    .map((line) => JSON.parse(line));

  var ss = SpreadsheetApp.getActiveSpreadsheet();
  // Replace with Sheet Name to be updated
  var sheet = ss.getSheetByName(SHEET_NAME);

  // 3. Define the desired column headers and their corresponding JSON paths
  let columnMapping = [
    { header: 'Run ID', path: 'runId' },
    { header: 'Platform', path: 'info.platform' },
    { header: 'Image', path: 'info.image' },
    { header: 'Instance', path: 'info.instance' },
    { header: 'Zone', path: 'info.zone' },
    { header: 'Machine Type', path: 'info.machineType' },
    { header: 'CPUs', path: 'info.hardwareThreadCount' },
    { header: 'Linux kernel', path: 'info.linuxKernel' },
    { header: 'Burst Size', path: 'params.burstSize' },
    { header: 'QPS', path: 'params.queriesPerSecond' },
    { header: 'Num Bursts', path: 'params.queryCount' },
    { header: 'Num Workers', path: 'params.numWorkers' },
    { header: 'Total Elapsed Time (s)', path: 'statistics.totalElapsedTime', transform: (val) => val.replace('s', '') },
    { header: 'Total Invocation Count', path: 'statistics.totalInvocationCount' },
    { header: 'Failure Count', path: 'statistics.failureCount' },
    { header: 'Failure Pct', path: 'statistics.failurePct' },
    { header: 'Late Count', path: 'statistics.lateCount' },
    { header: 'Late Burst Pct', path: 'statistics.lateBurstPct' },
    { header: 'Burst Latencies min (ms)', path: 'burstLatencies.min', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies p50 (ms)', path: 'burstLatencies.p50', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies p90 (ms)', path: 'burstLatencies.p90', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies p95 (ms)', path: 'burstLatencies.p95', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies p99 (ms)', path: 'burstLatencies.p99', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Latencies max (ms)', path: 'burstLatencies.max', transform: (val) => secondsToMilliseconds(val) },
    {
      header: 'Invocation Latencies min (ms)',
      path: 'invocationLatencies.min',
      transform: (val) => secondsToMilliseconds(val),
    },
    {
      header: 'Invocation Latencies p50 (ms)',
      path: 'invocationLatencies.p50',
      transform: (val) => secondsToMilliseconds(val),
    },
    {
      header: 'Invocation Latencies p90 (ms)',
      path: 'invocationLatencies.p90',
      transform: (val) => secondsToMilliseconds(val),
    },
    {
      header: 'Invocation Latencies p95 (ms)',
      path: 'invocationLatencies.p95',
      transform: (val) => secondsToMilliseconds(val),
    },
    {
      header: 'Invocation Latencies p99 (ms)',
      path: 'invocationLatencies.p99',
      transform: (val) => secondsToMilliseconds(val),
    },
    {
      header: 'Invocation Latencies max (ms)',
      path: 'invocationLatencies.max',
      transform: (val) => secondsToMilliseconds(val),
    },
  ];

  if (jsonData[0].outputLatencies?.min) {
    columnMapping.push({
      header: 'Output Latencies min (ms)',
      path: 'outputLatencies.min',
      transform: (val) => secondsToMilliseconds(val),
    });
    columnMapping.push({
      header: 'Output Latencies p50 (ms)',
      path: 'outputLatencies.p50',
      transform: (val) => secondsToMilliseconds(val),
    });
    columnMapping.push({
      header: 'Output Latencies p90 (ms)',
      path: 'outputLatencies.p90',
      transform: (val) => secondsToMilliseconds(val),
    });
    columnMapping.push({
      header: 'Output Latencies p95 (ms)',
      path: 'outputLatencies.p95',
      transform: (val) => secondsToMilliseconds(val),
    });
    columnMapping.push({
      header: 'Output Latencies p99 (ms)',
      path: 'outputLatencies.p99',
      transform: (val) => secondsToMilliseconds(val),
    });
    columnMapping.push({
      header: 'Output Latencies max (ms)',
      path: 'outputLatencies.max',
      transform: (val) => secondsToMilliseconds(val),
    });
  }

  // 4. Prepare the data
  var data = [];
  data.push(columnMapping.map((col) => col.header)); // Add headers

  jsonData.forEach(function (record) {
    var row = columnMapping.map((col) => {
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
  return path.split('.').reduce((current, key) => (current && current[key] !== undefined ? current[key] : ''), obj);
}
