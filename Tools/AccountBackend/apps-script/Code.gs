/**
 * Blackout Hunt — always-on feedback ingest on Google Apps Script.
 *
 * FREE: no server, no credit card, no extra account — it runs inside YOUR Google account.
 * It receives the game's feedback/telemetry POSTs, emails feedback to you (via your own
 * Gmail — no SMTP or app password needed), and logs everything to a Google Sheet that
 * doubles as your dashboard. Always-on (Google's infrastructure).
 *
 * DEPLOY (~3 minutes):
 *   1. Go to https://script.google.com  ->  New project. Delete the sample code, paste this file.
 *   2. Save (disk icon). Then "Deploy" -> "New deployment".
 *   3. Click the gear -> "Web app". Set: Execute as = Me,  Who has access = Anyone. Deploy.
 *   4. Authorize when prompted (it needs send-email + Sheets). Approve.
 *   5. Copy the "Web app" URL (ends in /exec).
 *   6. Your FeedbackBackendBaseUrl is that URL with "?path=" appended, e.g.
 *        https://script.google.com/macros/s/.../exec?path=
 *      (the game adds /feedback or /telemetry/session as the query value). Set it in
 *      Config/DefaultGame.ini and re-cook.
 *
 * Updating later: paste new code, then Deploy -> Manage deployments -> edit (pencil) ->
 * Version: New version -> Deploy. That keeps the SAME /exec URL.
 *
 * A "BlackoutHunt Feedback" spreadsheet appears in your Drive on the first submission.
 * Notes: consumer Gmail sends ~100 emails/day; everything is still logged to the Sheet even
 * if that cap is hit. Surveys are logged but not emailed (see EMAIL_KINDS).
 */

// Which feedback kinds to email you (all kinds are logged to the Sheet regardless).
// Default skips bulk end-of-round surveys.
var EMAIL_KINDS = ['bug', 'idea', 'praise', 'other'];
var SPREADSHEET_NAME = 'BlackoutHunt Feedback';
// Bump this on each change. GET the /exec URL to read it back and confirm which code is live.
var SCRIPT_VERSION = '2026-05-30-specs';

function doPost(e) {
  try {
    var body = {};
    if (e && e.postData && e.postData.contents) {
      try { body = JSON.parse(e.postData.contents); } catch (parseErr) { body = {}; }
    }
    // The game posts to <exec>?path=/feedback or <exec>?path=/telemetry/session (a path
    // SUFFIX on /exec 401s in Apps Script, but a query value reaches doPost). Route by that
    // query value, with a body-shape fallback (telemetry has no "kind").
    var route = '';
    if (e && e.parameter && e.parameter.path) route = String(e.parameter.path);
    else if (e && e.pathInfo) route = String(e.pathInfo);
    var looksTelemetry = route.indexOf('telemetry') >= 0 ||
      (!body.kind && (body.reason || body.round_results || body.perf));
    if (looksTelemetry) {
      logTelemetry_(body);
    } else {
      logFeedback_(body);
      var kind = String(body.kind || 'other');
      if (EMAIL_KINDS.indexOf(kind) >= 0) {
        emailFeedback_(kind, body);
      }
    }
    return jsonOut_({ ok: true });
  } catch (err) {
    return jsonOut_({ ok: false, error: String(err) });
  }
}

function doGet(e) {
  return jsonOut_({ ok: true, service: 'blackouthunt-feedback', version: SCRIPT_VERSION, hint: 'POST /feedback or /telemetry/session' });
}

function jsonOut_(obj) {
  return ContentService.createTextOutput(JSON.stringify(obj)).setMimeType(ContentService.MimeType.JSON);
}

function spreadsheet_() {
  var props = PropertiesService.getScriptProperties();
  var id = props.getProperty('SHEET_ID');
  if (id) {
    try { return SpreadsheetApp.openById(id); } catch (ignore) { /* fall through and recreate */ }
  }
  var ss = SpreadsheetApp.create(SPREADSHEET_NAME);
  props.setProperty('SHEET_ID', ss.getId());
  return ss;
}

function tab_(name, headers) {
  var ss = spreadsheet_();
  var sh = ss.getSheetByName(name);
  if (!sh) {
    sh = ss.insertSheet(name);
    sh.appendRow(headers);
    sh.setFrozenRows(1);
    return sh;
  }
  // Keep the header in sync if the schema grew (e.g. the PC-spec columns added later),
  // so an already-created sheet gets the new column labels on the next write.
  var firstRow = sh.getRange(1, 1, 1, Math.max(sh.getLastColumn(), headers.length)).getValues()[0];
  if (String(firstRow[headers.length - 1] || '') === '') {
    sh.getRange(1, 1, 1, headers.length).setValues([headers]);
    sh.setFrozenRows(1);
  }
  return sh;
}

function logFeedback_(b) {
  var c = b.context || {};
  var d = b.diagnostics || {};
  var dev = d.device || {};
  var perf = d.perf || {};
  var perfStr = perf.avg_fps ? ('avg ' + perf.avg_fps + ' / min ' + perf.min_fps + ' / 1%low ' + (perf.p1_low_fps || '') + ' / ' + (perf.hitches || 0) + ' hitches') : '';
  tab_('feedback', ['received_at', 'kind', 'rating', 'message', 'contact', 'player', 'level', 'role', 'app_version', 'platform', 'cpu', 'gpu', 'ram_gb', 'os', 'perf'])
    .appendRow([new Date(), b.kind || '', b.rating || '', b.message || '', b.contact || '',
                c.player_name || '', c.level || '', c.role || '', c.app_version || '', c.platform || '',
                dev.cpu || '', dev.gpu || '', dev.ram_gb || '', dev.os || '', perfStr]);
}

function logTelemetry_(b) {
  var perf = b.perf ? ('avg ' + b.perf.avg_fps + ' / min ' + b.perf.min_fps + ' / ' + b.perf.hitches + ' hitches') : '';
  var rounds = Array.isArray(b.round_results)
    ? b.round_results.map(function (r) { return (r.level || '?') + ':' + (r.role || '?') + '->' + (r.result || '?'); }).join(' | ')
    : '';
  tab_('telemetry', ['received_at', 'reason', 'app_version', 'platform', 'session_id', 'rounds', 'perf'])
    .appendRow([new Date(), b.reason || '', b.app_version || '', b.platform || '', b.session_id || '', rounds, perf]);
}

function emailFeedback_(kind, b) {
  var to = Session.getEffectiveUser().getEmail(); // you — the account running this script
  if (!to) return;
  var c = b.context || {};
  var d = b.diagnostics || {};
  var dev = d.device || {};
  var perf = d.perf || {};
  var snippet = String(b.message || 'feedback').replace(/\s+/g, ' ').slice(0, 80);
  var subject = '[Blackout Hunt] ' + kind + (b.rating ? ' ' + b.rating + '★' : '') + ' — ' + snippet;
  var lines = [
    'Kind:    ' + kind,
    'Rating:  ' + (b.rating || '-') + '/5',
    'From:    ' + (c.player_name || 'anonymous') + (b.contact ? ' (' + b.contact + ')' : ''),
    'Context: ' + [c.app_version && ('v' + c.app_version), c.platform, c.level, c.role].filter(Boolean).join(' · ')
  ];
  // PC specs + performance — present only when the player left "Include diagnostics" ticked (default on).
  var pc = [dev.cpu, dev.gpu, (dev.ram_gb ? dev.ram_gb + 'GB' : ''), dev.os].filter(Boolean).join(' · ');
  if (pc) lines.push('PC:      ' + pc);
  if (perf.avg_fps) lines.push('Perf:    FPS avg ' + perf.avg_fps + ' / min ' + perf.min_fps + ' / 1%low ' + (perf.p1_low_fps || '-') + ' · ' + (perf.hitches || 0) + ' hitches');
  lines.push('', b.message || '', '', '— Blackout Hunt (Apps Script)');
  MailApp.sendEmail(to, subject, lines.join('\n'));
}
