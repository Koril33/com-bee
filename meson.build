project(
  'ComBee', 'c',
  version: '0.0.1',
)

gtk4_dep = dependency('gtk4', version : '>=4.14.0')
serialport_dep = dependency('libserialport')

executable(
  'ComBee',
  'src/main.c',
  dependencies : [gtk4_dep, serialport_dep],
  install : true
)

