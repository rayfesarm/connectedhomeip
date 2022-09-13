## Example scripts for use with Arm Virtual Hardware (AVH)


Example workflows (`avh*.yml`) for use with [Arm Virtual Hardware](https://avh.arm.com/) provided in `.github/workflows`, which use these scripts to control third party virtual hardware. Requires an Arm login.

To select target, uncomment the appropriate `MODEL` in the yml file. For example to select `Raspberry Pi 4`:
```
        env:
          # MODEL: imx8mp-evk
          MODEL: rpi4b
...
```

You also need to save your personal `API_TOKEN` (from `AVH > Profile > API`) as a (`Settings > Secrets > Actions > New repository secret`) Secret, so that the workflow can access your AVH account.
```
        env:
... 
          API_TOKEN: ${{ secrets.API_TOKEN }}
```
