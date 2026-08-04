// FanFolder — Select 28 Store Languages in Partner Center
// Paste into browser console (F12 → Console) on the language picker page
// Selects: English, Arabic, Chinese, Czech, Danish, Dutch, Finnish, French,
//   German, Greek, Hebrew, Hindi, Hungarian, Indonesian, Italian, Japanese,
//   Korean, Norwegian, Polish, Portuguese, Romanian, Russian, Kiswahili,
//   Swedish, Thai, Turkish, Ukrainian, Vietnamese
// Skips: Spanish, Chinese (Simplified), Chinese (Traditional), etc.
(function() {
  const wanted = [
    "English", "Arabic", "Chinese", "Czech", "Danish", "Dutch",
    "Finnish", "French", "German", "Greek", "Hebrew", "Hindi",
    "Hungarian", "Indonesian", "Italian", "Japanese", "Korean",
    "Norwegian", "Polish", "Portuguese", "Romanian", "Russian",
    "Kiswahili", "Swedish", "Thai", "Turkish", "Ukrainian", "Vietnamese"
  ];
  const wantedSet = new Set(wanted.map(l => l.toLowerCase()));
  let checked = [], skipped = [], already = [];

  document.querySelectorAll('.item-checkbox').forEach(function(item) {
    const span = item.querySelector('span[title]');
    if (!span) return;
    const langName = span.getAttribute('title').trim();
    const input = item.querySelector('input[type="checkbox"]');
    if (!input) return;
    if (wantedSet.has(langName.toLowerCase())) {
      if (input.checked) {
        already.push(langName);
      } else {
        input.click();
        checked.push(langName);
      }
    } else {
      skipped.push(langName);
    }
  });

  console.log(`%c FanFolder Language Selection `, 'background:#0078D4;color:white;padding:2px 6px;border-radius:3px');
  console.log(`✅ Checked ${checked.length}: ${checked.join(', ')}`);
  console.log(`☑️  Already checked ${already.length}: ${already.join(', ')}`);
  console.log(`⏭️  Skipped ${skipped.length}: ${skipped.join(', ')}`);
  console.log(`Total in picker: ${checked.length + already.length + skipped.length}`);
  const missing = wanted.filter(l => !checked.concat(already).some(c => c.toLowerCase() === l.toLowerCase()));
  if (missing.length) console.warn(`⚠️ Not found: ${missing.join(', ')}`);
  else console.log(`🎯 All 28 languages selected!`);
})();
