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
 * {"info":{"platform":"AWS Nitro","image":"privacysandbox/roma-byob/traffic-generator","instance":"ami-0ebfd941bbafe70c6","machineType":"m5.16xlarge","zone":"us-east-1","timestamp":"2025-03-27T19:13:53.530221955+00:00","commitHash":"263efb40364edb126008c6ebaba4f14ecc8e9545","hardwareThreadCount":32,"memory":0,"linuxKernel":"6.6.79","rlimitSigpending":{"soft":256142,"hard":256142}},"runId":"00000008-F2DD-8A5F-4D9E-7BCDA56DC85A","params":{"burstSize":"6","queriesPerSecond":650,"queryCount":20000,"sandboxEnabled":false,"numWorkers":64,"lateBurstThreshold":0,"functionName":"PrimeSieve","inputArgs":"10000000"},"statistics":{"totalElapsedTime":"30.768110104s","totalInvocationCount":"120000","lateCount":"530","lateBurstPct":2.7,"failureCount":"109017","failurePct":90.8},"burstCreationLatencies":{"count":"20000","min":"0.000073814s","p50":"0.000345777s","p90":"0.000374313s","p95":"0.000393779s","p99":"0.000950510s","max":"0.010049279s"},"burstProcessingLatencies":{"count":"20000","min":"0.000304822s","p50":"0.000362302s","p90":"0.191205569s","p95":"0.204110739s","p99":"0.229378725s","max":"0.515448337s"},"invocationLatencies":{"count":"10983","min":"0.081124017s","p50":"0.170332251s","p90":"0.203817798s","p95":"0.215363343s","p99":"0.242849578s","max":"0.515446572s"},"waitLatencies":{"count":"7077","min":"0.000000144s","p50":"0.000000826s","p90":"0.000051224s","p95":"0.000059090s","p99":"0.000504364s","max":"0.009723860s"}}
 * {"info":{"platform":"AWS Nitro","image":"privacysandbox/roma-byob/traffic-generator","instance":"ami-0ebfd941bbafe70c6","machineType":"m5.16xlarge","zone":"us-east-1","timestamp":"2025-03-27T19:10:43.701660798+00:00","commitHash":"263efb40364edb126008c6ebaba4f14ecc8e9545","hardwareThreadCount":32,"memory":0,"linuxKernel":"6.6.79","rlimitSigpending":{"soft":256142,"hard":256142}},"runId":"00000008-EF09-9900-2B89-28CD4100B6E1","params":{"burstSize":"6","queriesPerSecond":650,"queryCount":20000,"sandboxEnabled":false,"numWorkers":64,"lateBurstThreshold":0,"functionName":"PrimeSieve","inputArgs":"1000000"},"statistics":{"totalElapsedTime":"30.768246449s","totalInvocationCount":"120000","lateCount":"1070","lateBurstPct":5.4,"failureCount":"29707","failurePct":24.8},"burstCreationLatencies":{"count":"20000","min":"0.000047334s","p50":"0.000109112s","p90":"0.000354241s","p95":"0.000589129s","p99":"0.002842402s","max":"0.008423407s"},"burstProcessingLatencies":{"count":"20000","min":"0.000314738s","p50":"0.016577809s","p90":"0.020796218s","p95":"0.022186489s","p99":"0.025190154s","max":"0.033702611s"},"invocationLatencies":{"count":"90293","min":"0.005138206s","p50":"0.012718951s","p90":"0.017755207s","p95":"0.019178786s","p99":"0.022343397s","max":"0.033667144s"},"waitLatencies":{"count":"18581","min":"0.000000154s","p50":"0.000000252s","p90":"0.000001132s","p95":"0.000037580s","p99":"0.000064411s","max":"0.004880853s"}}
 * {"info":{"platform":"AWS Nitro","image":"privacysandbox/roma-byob/traffic-generator","instance":"ami-0ebfd941bbafe70c6","machineType":"m5.16xlarge","zone":"us-east-1","timestamp":"2025-03-27T19:20:09.581404505+00:00","commitHash":"263efb40364edb126008c6ebaba4f14ecc8e9545","hardwareThreadCount":32,"memory":0,"linuxKernel":"6.6.79","rlimitSigpending":{"soft":256142,"hard":256142}},"runId":"00000008-ED4C-0A0D-67C7-B912055482E3","params":{"burstSize":"6","queriesPerSecond":650,"queryCount":20000,"sandboxEnabled":false,"numWorkers":64,"lateBurstThreshold":0,"functionName":"PrimeSieve","inputArgs":"2000000"},"statistics":{"totalElapsedTime":"30.768089520s","totalInvocationCount":"120000","lateCount":"528","lateBurstPct":2.6,"failureCount":"68897","failurePct":57.4},"burstCreationLatencies":{"count":"20000","min":"0.000047854s","p50":"0.000268855s","p90":"0.000358131s","p95":"0.000385565s","p99":"0.001486525s","max":"0.008438703s"},"burstProcessingLatencies":{"count":"20000","min":"0.000298207s","p50":"0.033610643s","p90":"0.041866642s","p95":"0.044512963s","p99":"0.049070780s","max":"0.129705479s"},"invocationLatencies":{"count":"51103","min":"0.013523860s","p50":"0.031606921s","p90":"0.038295646s","p95":"0.041174297s","p99":"0.046761215s","max":"0.129692210s"},"waitLatencies":{"count":"15523","min":"0.000000161s","p50":"0.000000481s","p90":"0.000033954s","p95":"0.000056932s","p99":"0.000068929s","max":"0.007039437s"}}
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
    { header: 'Function Name', path: 'params.functionName' },
    { header: 'Input Args', path: 'params.inputArgs' },
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
