#!/bin/bash

TEMPLATE="SlickTemplate.qss"

function makeStyle(){
  cat "$1" | sed \
    -e "s/%background_color%/$2/g" \
    -e "s/%foreground_color%/$3/g" \
    -e "s/%button_color%/$4/g" \
    -e "s/%scroll_color%/$5/g" \
    -e "s/%selection_background_color%/$6/g" > "${7}"
}

#makeStyle            bg       fg       button   scroll   selection output
makeStyle "$TEMPLATE" "fcfcfc" "202020" "e0e0e0" "e0e0e0" "e0e0e0" "$1/Spotlight.qss"
makeStyle "$TEMPLATE" "030303" "d0d0d0" "404040" "404040" "404040" "$1/Spotlight Dark.qss"

