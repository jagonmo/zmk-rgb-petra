# zmk-rgb-petra

Módulo ZMK que reemplaza el underglow nativo con RGB per-key reactivo
para el teclado Petra (shield flatcat, nice!nano v2).

## Uso

En tu `config/west.yml` del zmk-config, agrega el remote y el proyecto:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: jagonmo
      url-base: https://github.com/jagonmo
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-rgb-petra
      remote: jagonmo
      revision: main
  self:
    path: config
```

Luego en tu `.conf`:

```
CONFIG_RGB_PETRA=y
```

## Efectos

Cicleables con `&rgb_petra RGBP_EFF 0`: solid, breathe, spectrum, swirl,
reactive-flash, reactive-trail, layer-color.
