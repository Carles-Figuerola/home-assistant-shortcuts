// Clay configuration page layout
// Icon options must match ICON_* resources in package.json (index = resource order)

var ICON_OPTIONS = [
  { label: 'Star',   value: '0' },
  { label: 'Unlock', value: '1' },
  { label: 'Lock',   value: '2' },
  { label: 'Light',  value: '3' },
  { label: 'Garage', value: '4' },
  { label: 'Scene',  value: '5' },
  { label: 'Power',  value: '6' },
  { label: 'Home',   value: '7' }
];

function shortcutSection(index) {
  return {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Shortcut ' + (index + 1)
      },
      {
        type: 'input',
        messageKey: 'Name' + index,
        label: 'Name',
        defaultValue: '',
        attributes: {
          placeholder: 'e.g. Unlock Front Door',
          limit: 24
        }
      },
      {
        type: 'input',
        messageKey: 'Script' + index,
        label: 'Script',
        defaultValue: '',
        attributes: {
          placeholder: 'e.g. unlock_front_door'
        }
      },
      {
        type: 'select',
        messageKey: 'Icon' + index,
        label: 'Icon',
        defaultValue: '0',
        options: ICON_OPTIONS
      }
    ]
  };
}

var config = [
  {
    type: 'heading',
    defaultValue: 'Home Assistant Shortcuts'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Connection'
      },
      {
        type: 'input',
        messageKey: 'BaseUrl',
        label: 'Base URL',
        defaultValue: 'https://your-hostname/api/services/script/',
        attributes: {
          placeholder: 'https://your-ha-instance/api/services/script/'
        }
      },
      {
        type: 'input',
        messageKey: 'Token',
        id: 'tokenInput',
        label: 'Bearer Token',
        defaultValue: '',
        attributes: {
          placeholder: 'Long-lived access token',
          type: 'password'
        }
      },
      {
        type: 'toggle',
        id: 'showToken',
        label: 'Show Token',
        defaultValue: false
      }
    ]
  }
];

// Add 8 shortcut slots
for (var i = 0; i < 8; i++) {
  config.push(shortcutSection(i));
}

config.push({
  type: 'submit',
  defaultValue: 'Save Settings'
});

module.exports = config;
