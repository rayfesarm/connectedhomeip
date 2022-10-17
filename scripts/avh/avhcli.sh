#!/usr/bin/env bash

# Global variables
AVH_TOKEN=""
BEARER=""
PROJECT=""
INSTANCE=""
MODEL="rpi4b"
IP=""
NAME="name"

# Static variables
AVH_URL="https://app.avh.arm.com/api/v1"
BASEDIR=$(dirname "$0")

# Usage info
show_help() {
cat << EOF
Usage: ${0##*/} [--help | -h] [--token | -t TOKEN] [--name | -n NAME] [--model | -m MODEL] [--id | -i ID] OPERATION
CLI tool for Arm Virtual Hardware.
    --help  | -h         display this help and exit
    --token | -t TOKEN   specify API token
    --name  | -n NAME    specify an instance name for creation or if already created but no local id file is stored
                         i.e. running in a GitHub workflow
    --model | -m MODEL   specify AVH model when using create. Ignored otherwise
                         MODEL should be one of imx8mp-evk, rpi4b(default) or stm32u5-b-u585i-iot02a
    --id    | -i ID      specify Arm Virtual Hardware instance ID when using start, stop or delete. Ignored otherwise
                         Instance ID can be obtained using status                         
    OPERATION should be one of:
    create | -c          create an Arm Virtual Hardware instance
    delete | -d          delete the Arm Virtual Hardware instance create by this script
    start  | -l          start the Arm Virtual Hardware instance created by this script
    stop   | -s          stop the Arm Virtual Hardware instance created by this script
    status | -q          query status of the Arm Virtual Hardware instances    
EOF
}

# Create settings folder
mkdir -p $BASEDIR/.avh

# Get token from settings folder or ask the user
get_token() {
  if [ "$AVH_TOKEN" == "" ]; then
    if [ -f $BASEDIR/.avh/token.txt ]; then
      AVH_TOKEN=$(cat $BASEDIR/.avh/token.txt)
      echo "Using token found in $BASEDIR/.avh/token.txt"
      return
    else
      echo "Token not found in $BASEDIR/.avh/avh_token.txt. Please enter token:"
      read AVH_TOKEN
      echo "Saving token in $BASEDIR/.avh/token.txt"
      echo $AVH_TOKEN > $BASEDIR/.avh/token.txt
    fi
  fi
}

# Get bearer token
get_bearer() {
  REQ="{\"apiToken\": \"$AVH_TOKEN\"}"
  BEARER=$(curl -s -X POST "$AVH_URL/auth/login" \
    -H "Accept: application/json" \
    -H "Content-Type: application/json" \
    -d "$REQ" \
    | jq -r '.token' )
}

# Get project
get_project() {
  PROJECT=$(curl -s -X GET "$AVH_URL/projects" \
    -H "Accept: application/json" \
    -H "Authorization: Bearer $BEARER" \
    | jq -r '.[]' | jq -r '.id' )
}

# Get instance IP
get_ip() {
  if [ "$INSTANCE" == "" ]; then
    if [ -f $BASEDIR/.avh/$NAME.txt ]; then
      INSTANCE=$(cat $BASEDIR/.avh/$NAME.txt)
    else
      return
    fi
  fi
  IP=$(curl -s -X GET "$AVH_URL/instances" \
    -H "Accept: application/json" \
    -H "Authorization: Bearer $BEARER" \
    | jq -r ".[] | select(.id==\"$INSTANCE\")" | jq -r '.serviceIp' )
  LANIP=$(curl -s -X GET "$AVH_URL/instances" \
    -H "Accept: application/json" \
    -H "Authorization: Bearer $BEARER" \
    | jq -r ".[] | select(.id==\"$INSTANCE\")" | jq -r '.wifiIp' )
  echo $IP > $BASEDIR/"$NAME"_ip.txt
  echo $LANIP > $BASEDIR/"$NAME"_lan_ip.txt
  echo "Instance Service IP is: $IP"
  echo "Instance LAN IP is: $LANIP"
  echo "To connect to the console: nc $IP 2000"
  echo "To ssh in, use: ssh <username>@$LANIP"
  echo "IP information has been saved in $BASEDIR/"$NAME"_ip.txt and $BASEDIR/"$NAME"_lan_ip.txt"
}

# Open console 
get_console() { 
  echo Opening console to $NAME $INSTANCE
  CONSOLE=$(curl -s -X GET "$AVH_URL/instances/$INSTANCE/console" \
    -H "Accept: application/json" \
    -H "Authorization: Bearer $BEARER" \
    | jq -r ".[]" )
  echo $CONSOLE > $BASEDIR/"$NAME"_console.txt
  #echo $CONSOLE|fgrep url|cut -d '"' -f4 > $BASEDIR/"$NAME"_console.txt
  echo "Web console URL saved in $BASEDIR/"$NAME"_console.txt"
}

# Get ovpn certificate
get_ovpn() {
  if [ "$INSTANCE" == "" ]; then
    if [ -f $BASEDIR/.avh/$NAME.txt ]; then
      INSTANCE=$(cat $BASEDIR/.avh/$NAME.txt)
    else
      return
    fi
  fi
  curl -s -X GET "$AVH_URL/projects/$PROJECT/vpnconfig/ovpn" \
    -H "Accept: application/json" \
    -H "Authorization: Bearer $BEARER" > $BASEDIR/avh.ovpn
  echo "OpenVPN certificate has been saved in $BASEDIR/avh.ovpn"
}

# Create instance
create() {
  echo Creating $NAME...

  if [ ! -z "$INSTANCE" ]; then
    #If instance ID was passed in at command line
    ID="$INSTANCE"
  elif [ -f $BASEDIR/.avh/$NAME.txt ]; then
    echo "Instance details have been found locally."
    ID=$(echo $BASEDIR/.avh/$NAME.txt)  
  else
    #Check all instances online in case this is a first run on a GitHub workflow
    INSTANCES=$(curl -s -X GET "$AVH_URL/instances" \
      -H "Accept: application/json" \
      -H "Authorization: Bearer $BEARER" \
      | jq -r '.[]' )   

    #Find ID by name
    if [ $(echo $INSTANCES | jq -c | fgrep $NAME |wc -l) -gt 1 ]; then
      echo "WARNING! More than one instance of this namae was found, getting ID, and IPs of first instance found."
    fi
    ID=$(echo $INSTANCES | jq -c | fgrep $NAME |jq -r '.id' | head -n1)
  fi  
  
  #if echo $INSTANCES|fgrep -q $NAME; then 
  if [ ! -z "$ID" ]; then
    echo "An instance has already been created. Do you want to start/stop/delete it instead?"
    echo "ID: $ID"
    echo $ID > $BASEDIR/.avh/"$NAME.txt"
    return
  else
    if [ "$MODEL" == "imx8mp-evk" ]; then
      OS="2.2.0"
    fi
    if [ "$MODEL" == "rpi4b" ]; then
      OS="11.2.0"
    fi
    if [ "$MODEL" == "stm32u5-b-u585i-iot02a" ]; then
      OS="1.1.0"
    fi
    REQ="{\"project\":\"$PROJECT\",\"name\":\"$NAME\",\"flavor\":\"$MODEL\",\"os\":\"$OS\"}"
    INSTANCE=$(curl -s -X POST "$AVH_URL/instances" \
      -H "Accept: application/json" \
      -H "Authorization: Bearer $BEARER" \
      -H "Content-Type: application/json" \
      -d "$REQ" \
      | jq -r '.id' )
    echo $INSTANCE > $BASEDIR/.avh/"$NAME.txt"
  fi

  # Wait for instance to be ready
  CMD="curl -s -X GET \"$AVH_URL/instances/$INSTANCE/state\" \
    -H \"Accept: application/json\" \
    -H \"Authorization: Bearer $BEARER\" "
  echo "Waiting for instance to be ready"
  STATUS=$(eval $CMD)
  while [ "$STATUS" == "creating" ] ; do
    printf "\r|"
    sleep 2
    printf "\r/"
    sleep 2
    printf "\r-"
    sleep 2
    printf "\r\\"
    sleep 2
    STATUS=$(eval $CMD)
  done
  printf "\rInstance is ready\n"
}

# Start instance
start_instance() {
  if [ "$INSTANCE" == "" ]; then
    if [ ! -f $BASEDIR/.avh/$NAME.txt ]; then
      echo "An instance could not be found. Did you forget to specify the instance ID?"
      return
    else
      INSTANCE=$(cat $BASEDIR/.avh/$NAME.txt)
    fi
  fi
  curl -s -X POST "$AVH_URL/instances/$INSTANCE/start" \
    -H "Accept: application/json" \
    -H "Authorization: Bearer $BEARER" \
    -H "Content-Type: application/json" \
    -d '{"paused":false}'
  # Wait for instance to be ready
  CMD="curl -s -X GET \"$AVH_URL/instances/$INSTANCE/state\" \
    -H \"Accept: application/json\" \
    -H \"Authorization: Bearer $BEARER\" "
  echo "Waiting for instance to be ready"
  STATUS=$(eval $CMD)
  while [ "$STATUS" != "on" ] ; do
    printf "\r|"
    sleep 2
    printf "\r/"
    sleep 2
    printf "\r-"
    sleep 2
    printf "\r\\"
    sleep 2
    STATUS=$(eval $CMD)
  done
  printf "\rInstance of $MODEL is ready\n"
}

# Stop instance
stop_instance() {
  if [ "$INSTANCE" == "" ]; then
    if [ ! -f $BASEDIR/.avh/$NAME.txt ]; then
      echo "An instance could not be found. Did you forget to specify the instance ID?"
      return
    else
      INSTANCE=$(cat $BASEDIR/.avh/$NAME.txt)
    fi
  fi
  curl -s -X POST "$AVH_URL/instances/$INSTANCE/stop" \
    -H "Accept: application/json" \
    -H "Authorization: Bearer $BEARER" \
    -H "Content-Type: application/json" \
    -d '{"soft":false}'
}

# Delete instance
delete() {
  if [ "$INSTANCE" == "" ]; then
    if [ ! -f $BASEDIR/.avh/$NAME.txt ]; then
      echo "An instance could not be found. Did you forget to specify the instance ID?"
      return
    else
      INSTANCE=$(cat $BASEDIR/.avh/$NAME.txt)
    fi
  fi
  curl -s -X DELETE "$AVH_URL/instances/$INSTANCE" \
    -H "Accept: application/json" \
    -H "Authorization: Bearer $BEARER"
   # Cleanup
  rm $BASEDIR/.avh/"$NAME.txt"
  rm $BASEDIR/"$NAME"_ip.txt
  rm $BASEDIR/"$NAME"_console.txt
  if [ -f $BASEDIR/avh.opvn ]; then
    rm $BASEDIR/avh.ovpn
  fi
}

# Status
get_status() {
  LIST=$(curl -s -X GET "$AVH_URL/projects/$PROJECT/instances" \
    -H "Accept: application/json" \
    -H "Authorization: Bearer $BEARER" \
    | jq -r '.[]')
  LIST_ID=( $( echo $LIST | jq -r '.id') )
  LIST_IP=( $( echo $LIST | jq -r '.serviceIp') )
  LIST_STATUS=( $( echo $LIST | jq -r '.state') )
  LIST_NAME=( $( echo $LIST | jq -r '.name') )
  printf "                 ID                  |     IP    | STATUS\t| NAME\n"
  printf "==================================================================================\n"
  ctr=0
  for i in ${LIST_ID[@]}; do
    printf "$i | ${LIST_IP[$ctr]} | ${LIST_STATUS[$ctr]}\t\t| ${LIST_NAME[$ctr]}\n"
    ctr=$(expr $ctr + 1)
  done
}

avh_create() {
  get_token
  get_bearer
  get_project
  echo "Creating instance"
  create
  get_ip
  get_console
  get_ovpn
}

avh_start() {
  get_token
  get_bearer
  get_project
  echo "Starting instance"
  start_instance
  get_ip
  get_console
  get_ovpn
}

avh_stop() {
  get_token
  get_bearer
  get_project
  echo "Stopping instance"
  stop_instance
}

avh_delete() {
  get_token
  get_bearer
  get_project
  echo "Deleting instance"
  delete
}

avh_status() {
  get_token
  get_bearer
  get_project
  echo "Querying status"
  get_status
}



#################
# Parse command #
#################

# Transform long options to short ones
for arg in "$@"; do
  shift
  case "$arg" in
    '--help')   set -- "$@" '-h'   ;;
    '--token')  set -- "$@" '-t'   ;;
    '--name')   set -- "$@" '-n'   ;;
    '--id')     set -- "$@" '-i'   ;;  
    '--model')  set -- "$@" '-m'   ;;
    'create')   set -- "$@" '-c'   ;;
    'delete')   set -- "$@" '-d'   ;;
    'start')    set -- "$@" '-l'   ;;
    'stop')     set -- "$@" '-s'   ;;
    'status')   set -- "$@" '-q'   ;;    
    *)          set -- "$@" "$arg" ;;
  esac
done

OPTIND=1
# Resetting OPTIND is necessary if getopts was used previously in the script.
# It is a good idea to make OPTIND local if you process options in a function.

while getopts ht:n:m:i:cdlsq opt; do
    case $opt in
        h)
            show_help
            exit 0
            ;;
        t)
            AVH_TOKEN=$OPTARG
            ;;
        n)
            NAME="$OPTARG"
            ;;   
        i)
            INSTANCE=$OPTARG
            if fgrep -q "$INSTANCE" .avh/*; then 
              NAME=$(fgrep "$INSTANCE" .avh/*|cut -d ":" -f1|cut -d "/" -f2|sed 's/\.txt//g')
            fi
            ;;                 
        m)
            MODEL=$OPTARG
            if [[ "$MODEL" != "imx8mp-evk" ]] && \
               [[ "$MODEL" != "rpi4b" ]] && \
               [[ "$MODEL" != "stm32u5-b-u585i-iot02a" ]]; then
              echo "Model is unknown"
              show_help
              exit 1
            fi
            ;;
        c)
            avh_create
            exit 0
            ;;
        d)
            avh_delete
            exit 0
            ;;
        l)
            avh_start
            exit 0
            ;;
        s)
            avh_stop
            exit 0
            ;;
        q)
            avh_status
            exit 0
            ;;            
        *)
            show_help >&2
            exit 1
            ;;
    esac
done
shift "$((OPTIND-1))"   # Discard the options and sentinel --

echo "I'm not sure what to do..."
echo "Please enter one of the following: create, start, stop, delete, status."
show_help
exit 1
