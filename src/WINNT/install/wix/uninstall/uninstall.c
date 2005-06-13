#define _WIN32_MSI 200
#include <windows.h>
#include <msi.h>

#define OAFW_UPGRADE_CODE TEXT("{6823EEDD-84FC-4204-ABB3-A80D25779833}")
int main(void)
{
    UINT rc = ERROR_SUCCESS;
    DWORD iProduct = 0;
    CHAR  szProductCode[39];

    MsiSetInternalUI(INSTALLUILEVEL_PROGRESSONLY, NULL);
    
    do {
        rc = MsiEnumRelatedProducts(OAFW_UPGRADE_CODE, 0, iProduct, szProductCode);
        if ( rc == ERROR_SUCCESS ) {
            MsiConfigureProduct(szProductCode, 0 /* ignored */, INSTALLSTATE_ABSENT);
            iProduct++;
        }
    } while ( rc == ERROR_SUCCESS );

    return (rc == ERROR_NO_MORE_ITEMS ? 0 : 1);
}
