from opsvalidator.base import BaseValidator
from opsvalidator import error
from opsvalidator.error import ValidationError
from opsrest.utils.utils import get_column_data_from_row


class PortVlanValidator(BaseValidator):
    resource = "port"

    def validate_modification(self, validation_args):
        portvlan_row = validation_args.resource_row
        portvlan_ip = get_column_data_from_row(portvlan_row, "ip4_address")
        portvlan_tag = get_column_data_from_row(portvlan_row, "tag")
        if (portvlan_ip != []) and (portvlan_tag != []):
            details = "L2 and L3 config cannot be applied on same interface"
            raise ValidationError(error.VERIFICATION_FAILED, details)
