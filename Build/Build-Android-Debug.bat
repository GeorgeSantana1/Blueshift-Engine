@pushd %~dp0
@cd Android\Blueshift
cmd /k gradlew.bat BlueshiftPlayer:assembleDebug
@popd