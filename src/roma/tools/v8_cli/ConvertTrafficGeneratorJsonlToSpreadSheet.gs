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
 * {"info":{"platform":"GCP CS","image":"privacysandbox/roma-byob/traffic-generator:v1-maxqps-24w-bs14-nq2000-minimal-lt15-noipc-root","instance":"gcp-xrbq-0adb92dbe2","machineType":"n2d-standard-32","zone":"us-central1-c","timestamp":"Tue, 25 Feb 2025 00:26:09 +0000","commitHash":"d7b45ea1ae1b9ed6c160024b0a860f6630d19960"},"runId":"00000000-0000-047E-339E-88B44DD7A13B","params":{"burstSize":"14","queriesPerSecond":5000,"queryCount":2000,"sandboxEnabled":false,"numWorkers":24,"lateBurstThreshold":15},"statistics":{"totalElapsedTime":"0.399898508s","totalInvocationCount":"28000","lateCount":"81","lateBurstPct":4.1,"failureCount":"24815","failurePct":88.6},"burstCreationLatencies":{"count":"2000","min":"0.000007260s","p50":"0.000024130s","p90":"0.000056020s","p95":"0.000070049s","p99":"0.000136439s","max":"0.000829314s"},"burstProcessingLatencies":{"count":"2000","min":"0.000014s","p50":"0.001944566s","p90":"0.002388264s","p95":"0.002569822s","p99":"0.003358077s","max":"0.011334750s"},"invocationLatencies":{"count":"3185","min":"0.000065980s","p50":"0.001987226s","p90":"0.002323444s","p95":"0.002473203s","p99":"0.003000568s","max":"0.011293811s"}}
 * {"info":{"platform":"GCP CS","image":"privacysandbox/roma-byob/traffic-generator:v1-maxqps-24w-bs14-nq2000-minimal-lt15-noipc-root","instance":"gcp-xrbq-0adb92dbe2","machineType":"n2d-standard-32","zone":"us-central1-c","timestamp":"Tue, 25 Feb 2025 00:26:09 +0000","commitHash":"d7b45ea1ae1b9ed6c160024b0a860f6630d19960"},"runId":"00000000-0000-047E-339E-88B44DD7A13B","params":{"burstSize":"14","queriesPerSecond":7500,"queryCount":2000,"sandboxEnabled":false,"numWorkers":24,"lateBurstThreshold":15},"statistics":{"totalElapsedTime":"0.266610496s","totalInvocationCount":"28000","lateCount":"245","lateBurstPct":12.3,"failureCount":"25869","failurePct":92.4},"burstCreationLatencies":{"count":"2000","min":"0.000005960s","p50":"0.000018179s","p90":"0.000044650s","p95":"0.000056500s","p99":"0.000091609s","max":"0.000691275s"},"burstProcessingLatencies":{"count":"2000","min":"0.000015251s","p50":"0.001792958s","p90":"0.002334424s","p95":"0.002498312s","p99":"0.003280237s","max":"0.009188565s"},"invocationLatencies":{"count":"2131","min":"0.000081200s","p50":"0.002006096s","p90":"0.002342534s","p95":"0.002479293s","p99":"0.003015638s","max":"0.009153015s"}}
 * {"info":{"platform":"GCP CS","image":"privacysandbox/roma-byob/traffic-generator:v1-maxqps-24w-bs14-nq2000-minimal-lt15-noipc-root","instance":"gcp-xrbq-0adb92dbe2","machineType":"n2d-standard-32","zone":"us-central1-c","timestamp":"Tue, 25 Feb 2025 00:26:09 +0000","commitHash":"d7b45ea1ae1b9ed6c160024b0a860f6630d19960"},"runId":"00000000-0000-047E-339E-88B44DD7A13B","params":{"burstSize":"14","queriesPerSecond":8750,"queryCount":2000,"sandboxEnabled":false,"numWorkers":24,"lateBurstThreshold":15},"statistics":{"totalElapsedTime":"0.228502001s","totalInvocationCount":"28000","lateCount":"452","lateBurstPct":22.6,"failureCount":"26190","failurePct":93.5},"burstCreationLatencies":{"count":"2000","min":"0.000006080s","p50":"0.000016390s","p90":"0.000045099s","p95":"0.000053870s","p99":"0.000082619s","max":"0.000596916s"},"burstProcessingLatencies":{"count":"2000","min":"0.000012060s","p50":"0.000044310s","p90":"0.002345074s","p95":"0.002459222s","p99":"0.002700251s","max":"0.003393437s"},"invocationLatencies":{"count":"1810","min":"0.000071469s","p50":"0.002068266s","p90":"0.002374333s","p95":"0.002462553s","p99":"0.002692781s","max":"0.003351547s"}}
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
    { header: 'Late Burst Threshold', path: 'params.lateBurstThreshold' },
    { header: 'Burst Creation Latencies min (ms)', path: 'burstCreationLatencies.min', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Creation Latencies p50 (ms)', path: 'burstCreationLatencies.p50', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Creation Latencies p90 (ms)', path: 'burstCreationLatencies.p90', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Creation Latencies p95 (ms)', path: 'burstCreationLatencies.p95', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Creation Latencies p99 (ms)', path: 'burstCreationLatencies.p99', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Creation Latencies max (ms)', path: 'burstCreationLatencies.max', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Processing Latencies min (ms)', path: 'burstProcessingLatencies.min', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Processing Latencies p50 (ms)', path: 'burstProcessingLatencies.p50', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Processing Latencies p90 (ms)', path: 'burstProcessingLatencies.p90', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Processing Latencies p95 (ms)', path: 'burstProcessingLatencies.p95', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Processing Latencies p99 (ms)', path: 'burstProcessingLatencies.p99', transform: (val) => secondsToMilliseconds(val) },
    { header: 'Burst Processing Latencies max (ms)', path: 'burstProcessingLatencies.max', transform: (val) => secondsToMilliseconds(val) },
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
