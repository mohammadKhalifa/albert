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
makeStyle "$TEMPLATE" "fcfcfc" "808080" "e0e0e0" "c0c0c0" "e0e0e0" "$1/Spotlight.qss"
makeStyle "$TEMPLATE" "030303" "808080" "202020" "404040" "202020" "$1/Spotlight Dark.qss"

