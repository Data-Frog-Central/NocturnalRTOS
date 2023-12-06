#!/bin/bash

. $BR2_CONFIG > /dev/null 2>&1
export BR2_CONFIG

#generate network upgrade package.
echo "**************************************************"
echo "Generate the network upgrade package"

DIR_PWD=$(pwd)
echo ${DIR_PWD}

DIR_WORK=$1
BR_CFG=$2 

DIR_NET_UPG=for-net-upgrade

if [ -z ${DIR_WORK} ] ; then
	DIR_WORK="."
fi

cd ${DIR_WORK}


UPGRADE_FILE=${IMAGES_DIR}/for-upgrade/${BR2_EXTERNAL_HCFOTA_FILENAME}

if [ ! -f ${UPGRADE_FILE} ] ; then
	echo "NO ${UPGRADE_FILE} !!!!"
	cd ${DIR_PWD}
	exit
else
	echo "Upgrade file:" ${UPGRADE_FILE}
fi

if [ -f vmlinux ] ; then
	OS_NAME=linux
	APP_PROJECTOR="PROJECTORAPP=y"
	APP_SCREEN="SCREENAPP=y"
else
	OS_NAME=rtos
	APP_PROJECTOR="APPS_PROJECTOR=y"
	APP_SCREEN="APPS_HCSCREEN=y"
	
	#rtos, then input parameter BR_CFG must be merged to be absolute address
	BR_CFG=${DIR_PWD}/${BR_CFG}
fi
#echo "pwd:" $(pwd) "BR_CFG" ${BR_CFG}

if [ -z ${BR_CFG} ] ; then
	APP_NAME=hcprojector
else
	APP_GET=$(grep -nr ${APP_SCREEN} ${BR_CFG})
	if [ -n "${APP_GET}" ] ; then
		APP_NAME=hcscreen
	fi
	APP_GET=$(grep -nr ${APP_PROJECTOR} ${BR_CFG})
	if [ -n "${APP_GET}" ] ; then
		APP_NAME=hcprojector
	fi
fi

PRODUCT_NAME=$(echo ${UPGRADE_FILE} | awk -F'_' '{print $2}')
PRODUCT_VERSION=$(echo ${UPGRADE_FILE} | awk -F'_' '{print $3}')
PRODUCT_VERSION=$(echo ${PRODUCT_VERSION} | awk -F'.' '{print $1}')




echo "OS_NAME:" ${OS_NAME}


echo "APP_NAME:" ${APP_NAME}

if [ -d ${DIR_NET_UPG} ] ; then
	rm -rf  ${DIR_NET_UPG}
fi

mkdir -p ${DIR_NET_UPG}

echo "work dir: $DIR_WORK"
	
#copy to HCFOTA_HC16A3000V104K_hcprojector_linux_2303271841.bin	
#UPGRADE_BIN=HCFOTA_${PRODUCT_NAME}_${APP_NAME}_${OS_NAME}_${PRODUCT_VERSION}.bin
#cp ${UPGRADE_FILE} ${DIR_NET_UPG}/${UPGRADE_BIN}
cp ${UPGRADE_FILE} ${DIR_NET_UPG}/

#here you can modify the url of upgreaded binary
BIN_URL=http://172.16.12.81:80/hccast/${OS_NAME}/${PRODUCT_NAME}/${APP_NAME}/${UPGRADE_FILE}


#UPGRADE_JSON=${PRODUCT_NAME}_${APP_NAME}_${OS_NAME}_upgrade_config.jsonp
UPGRADE_JSON=HCFOTA.jsonp
if [ -d ${DIR_NET_UPG} ] ; then
	echo "Generating upgrade json config ..."
	cat << EOF > ${DIR_NET_UPG}/${UPGRADE_JSON}
jsonp_callback({
  "product": "${PRODUCT_NAME}",
  "version": "${PRODUCT_VERSION}",
  "force_upgrade": true,
  "url": "${BIN_URL}"
})

EOF

	echo "Generating upgrade json config ${UPGRADE_JSON} done!"
fi

cd ${DIR_PWD}
echo "revert dir:" $(pwd)
echo "**************************************************"
